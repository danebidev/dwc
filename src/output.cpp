#include "output.hpp"

#include <algorithm>
#include <cassert>
#include <map>

#include "layer-shell.hpp"
#include "root.hpp"
#include "server.hpp"

namespace output {
    void new_output(wl_listener *listener, void *data) {
        wlr_output *wlr_output = static_cast<struct wlr_output *>(data);
        new Output(wlr_output);
        server.root.arrange();
    }

    void layout_update(wl_listener *listener, void *data) {
        wlr_output_configuration_v1 *config = wlr_output_configuration_v1_create();

        for(Output *output : server.root.outputs) {
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

    void apply_output_config(wlr_output_configuration_v1 *config, bool test) {
        std::map<std::string, OutputConfig *> config_map;

        struct wlr_output_configuration_head_v1 *config_head;
        wl_list_for_each(config_head, &config->heads, link) {
            std::string name = config_head->state.output->name;
            config_map[name] = new OutputConfig(config_head);
        }

        // Apply configs
        bool success = true;
        for(Output *output : server.root.outputs) {
            OutputConfig *oc = config_map[output->output->name];
            success &= output->apply_config(oc, test);
        }

        // Send config status
        if(success)
            wlr_output_configuration_v1_send_succeeded(config);
        else
            wlr_output_configuration_v1_send_failed(config);
    }

    OutputConfig::OutputConfig(wlr_output_configuration_head_v1 *config)
        : enabled(true),
          width(0),
          height(0),
          x(0.0),
          y(0.0),
          refresh(0.0),
          transform(WL_OUTPUT_TRANSFORM_NORMAL),
          scale(1.0),
          adaptive_sync(false) {
        enabled = config->state.enabled;

        if(config->state.mode != NULL) {
            struct wlr_output_mode *mode = config->state.mode;
            width = mode->width;
            height = mode->height;
            refresh = mode->refresh / 1000.f;
        }
        else {
            width = config->state.custom_mode.width;
            height = config->state.custom_mode.height;
            refresh = config->state.custom_mode.refresh / 1000.f;
        }
        x = config->state.x;
        y = config->state.y;
        transform = config->state.transform;
        scale = config->state.scale;
        adaptive_sync = config->state.adaptive_sync_enabled;
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
    void destroy(wl_listener *listener, void *data) {
        Output *output = static_cast<wrapper::Listener<Output> *>(listener)->container;
        server.root.outputs.remove(output);
        delete output;
    }

    Output::Output(wlr_output *output)
        : output(output),
          // Adds output to scene graph
          scene_output(wlr_scene_output_create(server.root.scene, output)),

          frame(this, output::frame, &output->events.frame),
          request_state(this, output::request_state, &output->events.request_state),
          destroy(this, output::destroy, &output->events.destroy) {
        output->data = this;

        // Configures the output to use our allocator and renderer
        wlr_output_init_render(output, server.allocator, server.renderer);

        server.root.outputs.push_back(this);

        // Enables the output if it's not enabled
        wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);

        // TODO: config
        wlr_output_mode *mode = wlr_output_preferred_mode(output);
        if(mode)
            wlr_output_state_set_mode(&state, mode);

        // Applies the state
        wlr_output_commit_state(output, &state);
        wlr_output_state_finish(&state);

        // Add the new output to the output layout
        // auto_add arranges outputs from left-to-right in the order they appear
        // TODO: config
        wlr_output_layout_output *layout_output =
            wlr_output_layout_add_auto(server.root.output_layout, output);

        layers.shell_background = wlr_scene_tree_create(server.root.shell_background);
        layers.shell_bottom = wlr_scene_tree_create(server.root.shell_bottom);
        layers.shell_top = wlr_scene_tree_create(server.root.shell_top);
        layers.shell_overlay = wlr_scene_tree_create(server.root.shell_overlay);

        // Add output to scene output layout
        wlr_scene_output_layout_add_output(server.scene_layout, layout_output, scene_output);

        server.root.arrange();
        update_position();
    }

    void Output::update_position() {
        wlr_output_layout_get_box(server.root.output_layout, output, &output_box);
    }

    void Output::arrange_layers() {
        wlr_box full_area = { 0 };
        wlr_output_effective_resolution(output, &full_area.width, &full_area.height);
        wlr_box usable_area = full_area;

        for(auto &layer : { layers.shell_overlay, layers.shell_top, layers.shell_bottom,
                            layers.shell_background }) {
            arrange_surface(&full_area, &usable_area, layer, true);
        }

        for(auto &layer : { layers.shell_overlay, layers.shell_top, layers.shell_bottom,
                            layers.shell_background }) {
            arrange_surface(&full_area, &usable_area, layer, false);
        }

        // TODO: handle focus
    }

    bool Output::apply_config(OutputConfig *config, bool test) {
        wlr_output *wlr_output = output;

        wlr_output_state state;
        wlr_output_state_init(&state);

        // enabled
        wlr_output_state_set_enabled(&state, config->enabled);

        if(config->enabled) {
            // set mode
            bool mode_set = false;
            if(config->width > 0 && config->height > 0 && config->refresh > 0) {
                // find matching mode
                struct wlr_output_mode *mode, *best_mode = nullptr;
                wl_list_for_each(mode, &wlr_output->modes, link) {
                    if(mode->width == config->width && mode->height == config->height)
                        if(!best_mode ||
                           (abs((int)(mode->refresh / 1000.0 - config->refresh)) < 1.5 &&
                            abs((int)(mode->refresh / 1000.0 - config->refresh)) <
                                abs((int)(best_mode->refresh / 1000.0 - config->refresh))))
                            best_mode = mode;
                }

                if(best_mode) {
                    wlr_output_state_set_mode(&state, best_mode);
                    mode_set = true;
                }
            }

            // set to preferred mode if not set
            if(!mode_set) {
                wlr_output_state_set_mode(&state, wlr_output_preferred_mode(wlr_output));
                wlr_log(WLR_INFO, "using fallback mode for output %s", config->name.c_str());
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
            success = wlr_output_test_state(wlr_output, &state);
        else {
            success = wlr_output_commit_state(wlr_output, &state);
            if(success) {
                wlr_output_layout_add(server.root.output_layout, wlr_output, config->x, config->y);
                update_position();
                arrange_layers();
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

    void Output::arrange_surface(wlr_box *full_area, wlr_box *usable_area, wlr_scene_tree *tree,
                                 bool exclusive) {
        wlr_scene_node *node;
        wl_list_for_each(node, &tree->children, link) {
            layer_shell::LayerSurface *surface =
                static_cast<layer_shell::LayerSurface *>(node->data);
            if(!surface || !surface->layer_surface->initialized)
                continue;
            if((surface->layer_surface->current.exclusive_zone > 0) != exclusive)
                continue;

            wlr_scene_layer_surface_v1_configure(surface->scene, full_area, usable_area);
        }
    }
}
