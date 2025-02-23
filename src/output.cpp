#include "output.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>

#include "layer-shell.hpp"
#include "root.hpp"
#include "server.hpp"

namespace output {
    void new_output(wl_listener *listener, void *data) {
        wlr_output *wlr_output = static_cast<struct wlr_output *>(data);
        new Output(wlr_output);
    }

    void layout_update(wl_listener *listener, void *data) {
        wlr_output_configuration_v1 *config = wlr_output_configuration_v1_create();

        for(Output *output : server.output_manager.outputs) {
            // Get the config head for each output
            assert(output->output);
            wlr_output_configuration_head_v1 *config_head =
                wlr_output_configuration_head_v1_create(config, output->output);

            struct wlr_box output_box;
            wlr_output_layout_get_box(server.root.output_layout, output->output, &output_box);

            // Mark the output enabled if it's swithed off but not disabled
            config_head->state.enabled = !wlr_box_empty(&output_box);
            config_head->state.x = output_box.x;
            config_head->state.y = output_box.y;
        }

        // Update the configuration
        wlr_output_manager_v1_set_configuration(server.output_manager_v1, config);
    }

    void output_test(wl_listener *listener, void *data) {
        server.output_manager.apply_output_config(static_cast<wlr_output_configuration_v1 *>(data),
                                                  true);
    }

    void output_apply(wl_listener *listener, void *data) {
        server.output_manager.apply_output_config(static_cast<wlr_output_configuration_v1 *>(data),
                                                  false);
    }

    // Called whenever an output wants to display a frame
    // Generally should be at the output's refresh rate
    void frame(wl_listener *listener, void *data) {
        Output *output = static_cast<wrapper::Listener<Output> *>(listener)->container;
        wlr_scene *scene = server.root.scene;
        wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->output);

        wlr_scene_output_commit(scene_output, nullptr);

        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        wlr_scene_output_send_frame_done(scene_output, &now);
    }

    // Called when the backend request a new state
    // For example, resizing a window in the X11 or wayland backend
    void request_state(wl_listener *listener, void *data) {
        Output *output = static_cast<wrapper::Listener<Output> *>(listener)->container;
        const wlr_output_event_request_state *event =
            static_cast<wlr_output_event_request_state *>(data);

        wlr_output_commit_state(output->output, event->state);

        output->arrange_layers();
        output->update_position();
        server.root.arrange();
    }

    // Called when an output is destroyed
    void output_destroy(wl_listener *listener, void *data) {
        Output *output = static_cast<wrapper::Listener<Output> *>(listener)->container;
        server.output_manager.outputs.remove(output);
        delete output;
    }

    Output::Output(wlr_output *output)
        : output(output),
          // Adds output to scene graph
          scene_output(wlr_scene_output_create(server.root.scene, output)),
          active_workspace(nullptr),

          frame(this, output::frame, &output->events.frame),
          request_state(this, output::request_state, &output->events.request_state),
          destroy(this, output::output_destroy, &output->events.destroy) {
        output->data = this;

        // Configures the output to use our allocator and renderer
        wlr_output_init_render(output, server.allocator, server.renderer);

        layers.shell_background = wlr_scene_tree_create(server.root.shell_background);
        layers.shell_bottom = wlr_scene_tree_create(server.root.shell_bottom);
        layers.shell_top = wlr_scene_tree_create(server.root.shell_top);
        layers.shell_overlay = wlr_scene_tree_create(server.root.shell_overlay);

        server.output_manager.outputs.push_back(this);

        config::OutputConfig *config = nullptr;
        if(conf.output_config.find(output->name) != conf.output_config.end())
            config = &conf.output_config[output->name];

        bool success = config && apply_config(config, false);
        if(!success) {
            wlr_log(WLR_INFO, "using fallback config for output %s", output->name);

            wlr_output_state state;
            wlr_output_state_init(&state);

            wlr_output_state_set_enabled(&state, true);
            wlr_output_state_set_mode(&state, wlr_output_preferred_mode(output));

            success = wlr_output_commit_state(output, &state);
            wlr_output_state_finish(&state);

            if(success) {
                // Add the new output to the output layout
                // auto_add arranges outputs from left-to-right in the order they appear
                wlr_output_layout_output *layout_output =
                    wlr_output_layout_add_auto(server.root.output_layout, output);

                // Add output to scene output layout
                wlr_scene_output_layout_add_output(server.scene_layout, layout_output,
                                                   scene_output);
            }
        }

        arrange_layers();
        update_position();

        workspaces.push_back(new workspace::Workspace(this));
        workspaces.front()->focus();
        active_workspace = workspaces.front();

        server.root.arrange();
    }

    Output::~Output() {
        for(const auto &ws : workspaces) delete ws;
    }

    void Output::update_position() {
        wlr_output_layout_get_box(server.root.output_layout, output, &output_box);
    }

    void Output::arrange_layers() {
        wlr_box full_area = { 0 };
        wlr_output_effective_resolution(output, &full_area.width, &full_area.height);
        usable_area = full_area;

        for(auto &layer : { layers.shell_overlay, layers.shell_top, layers.shell_bottom,
                            layers.shell_background }) {
            arrange_surface(&full_area, layer, true);
        }

        for(auto &layer : { layers.shell_overlay, layers.shell_top, layers.shell_bottom,
                            layers.shell_background }) {
            arrange_surface(&full_area, layer, false);
        }
    }

    bool Output::apply_config(config::OutputConfig *config, bool test) {
        wlr_output_state state;
        wlr_output_state_init(&state);

        // enabled
        wlr_output_state_set_enabled(&state, config->enabled);

        if(config->enabled) {
            // set mode
            bool mode_set = false;
            if(config->mode.has_value() && config->mode->width > 0 && config->mode->height > 0 &&
               config->mode->refresh_rate > 0) {
                Mode &config_mode = config->mode.value();
                // find matching mode
                struct wlr_output_mode *mode, *best_mode = nullptr;
                wl_list_for_each(mode, &output->modes, link) {
                    if(mode->width == config_mode.width && mode->height == config_mode.height)
                        if(!best_mode ||
                           (abs((int)(mode->refresh / 1000.0 - config_mode.refresh_rate)) < 1.5 &&
                            abs((int)(mode->refresh / 1000.0 - config_mode.refresh_rate)) <
                                abs((int)(best_mode->refresh / 1000.0 - config_mode.refresh_rate))))
                            best_mode = mode;
                }

                if(best_mode) {
                    wlr_output_state_set_mode(&state, best_mode);
                    mode_set = true;
                }
            }

            // set to preferred mode if not set
            if(!mode_set) {
                wlr_output_state_set_mode(&state, wlr_output_preferred_mode(output));
                wlr_log(WLR_INFO, "using fallback mode for output %s", output->name);
            }

            // scale
            if(config->scale > 0)
                wlr_output_state_set_scale(&state, config->scale);

            // transform
            wlr_output_state_set_transform(&state, config->transform);

            // adaptive sync
            wlr_output_state_set_adaptive_sync_enabled(&state, config->adaptive_sync);
        }

        bool success;
        if(test)
            success = wlr_output_test_state(output, &state);
        else {
            success = wlr_output_commit_state(output, &state);
            if(success) {
                if(config->pos.has_value())
                    wlr_output_layout_add(server.root.output_layout, output, config->pos->x,
                                          config->pos->y);
                else
                    wlr_output_layout_add_auto(server.root.output_layout, output);
            }
        }

        wlr_output_state_finish(&state);
        return success;
    }

    wlr_scene_tree *Output::get_scene(zwlr_layer_shell_v1_layer type) {
        switch(type) {
            case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
                return layers.shell_background;
            case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
                return layers.shell_bottom;
            case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
                return layers.shell_top;
            case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
                return layers.shell_overlay;
            default:
                wlr_log(WLR_ERROR, "invalid type? shouldn't happen");
                exit(EXIT_FAILURE);
        }
    }

    std::pair<double, double> Output::center() {
        double x = output_box.x + output_box.width / 2;
        double y = output_box.y + output_box.height / 2;
        return { x, y };
    }

    void Output::arrange_surface(wlr_box *full_area, wlr_scene_tree *tree, bool exclusive) {
        wlr_scene_node *node;
        wl_list_for_each(node, &tree->children, link) {
            layer_shell::LayerSurface *surface =
                static_cast<layer_shell::LayerSurface *>(node->data);
            if(!surface || !surface->layer_surface->initialized)
                continue;
            if((surface->layer_surface->current.exclusive_zone > 0) != exclusive)
                continue;

            wlr_scene_layer_surface_v1_configure(surface->scene, full_area, &usable_area);
        }
    }

    void output_layout_destroy(wl_listener *listener, void *data) {
        OutputManager *out = static_cast<wrapper::Listener<OutputManager> *>(listener)->container;
        out->layout_update.free();
        out->output_layout_destroy.free();
    }

    void output_manager_destroy(wl_listener *listener, void *data) {
        OutputManager *out = static_cast<wrapper::Listener<OutputManager> *>(listener)->container;
        out->output_test.free();
        out->output_apply.free();
        out->output_manager_destroy.free();
    }

    OutputManager::OutputManager(wl_display *display)
        : layout_update(this, output::layout_update, &server.root.output_layout->events.change),
          output_test(this, output::output_test, &server.output_manager_v1->events.test),
          output_apply(this, output::output_apply, &server.output_manager_v1->events.apply),

          output_layout_destroy(this, output::output_layout_destroy,
                                &server.root.output_layout->events.destroy),
          output_manager_destroy(this, output::output_manager_destroy,
                                 &server.output_manager_v1->events.destroy) {}

    Output *OutputManager::output_at(double x, double y) {
        wlr_output *output = wlr_output_layout_output_at(server.root.output_layout, x, y);
        return static_cast<Output *>(output->data);
    }

    Output *OutputManager::focused_output() {
        wlr_cursor *cursor = server.input_manager.seat.cursor.cursor;
        return output_at(cursor->x, cursor->y);
    }

    void OutputManager::apply_output_config(wlr_output_configuration_v1 *config, bool test) {
        struct wlr_output_configuration_head_v1 *config_head;
        wl_list_for_each(config_head, &config->heads, link) {
            conf.output_config[config_head->state.output->name] = config::OutputConfig(config_head);
        }

        // Apply configs
        bool success = true;
        for(Output *output : server.output_manager.outputs) {
            if(conf.output_config.find(output->output->name) != conf.output_config.end()) {
                config::OutputConfig *oc = &conf.output_config[output->output->name];
                success &= output->apply_config(oc, test);
                if(!test) {
                    output->arrange_layers();
                    output->update_position();
                    server.root.arrange();
                }
            }
        }

        // Send config status
        if(success)
            wlr_output_configuration_v1_send_succeeded(config);
        else
            wlr_output_configuration_v1_send_failed(config);
    }
}
