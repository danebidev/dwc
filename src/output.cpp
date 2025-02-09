#include "output.hpp"

#include <algorithm>

#include "layer-shell.hpp"
#include "server.hpp"

output::Output::Output(wlr_output *output)
    : output(output),
      frame(this, output::frame, &output->events.frame),
      request_state(this, output::request_state, &output->events.request_state),
      destroy(this, output::destroy, &output->events.destroy) {
    Server &server = Server::instance();

    // Configures the output to use our allocator and renderer
    wlr_output_init_render(output, server.allocator, server.renderer);

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
        wlr_output_layout_add_auto(server.output_layout, output);

    layers.shell_background = wlr_scene_tree_create(server.layers.shell_background);
    layers.shell_bottom = wlr_scene_tree_create(server.layers.shell_bottom);
    layers.shell_top = wlr_scene_tree_create(server.layers.shell_top);
    layers.shell_overlay = wlr_scene_tree_create(server.layers.shell_overlay);

    // Adds output to scene graph
    wlr_scene_output *scene_output = wlr_scene_output_create(server.scene, output);

    // Add output to scene output layout
    wlr_scene_output_layout_add_output(server.scene_layout, layout_output, scene_output);
}

void output::Output::arrange() {
    Server &server = Server::instance();

    wlr_box usable_area { 0 };
    wlr_output_effective_resolution(output, &usable_area.width, &usable_area.height);
    wlr_box full_area = usable_area;

    for(auto &layer : { server.layers.shell_background, server.layers.shell_bottom,
                        server.layers.shell_top, server.layers.shell_overlay }) {
        wlr_scene_node *node;
        wl_list_for_each(node, &layer->children, link) {
            layer_shell::LayerSurface *surface =
                static_cast<layer_shell::LayerSurface *>(node->data);
            if(!surface || !surface->layer_surface->initialized)
                continue;
            if(surface->layer_surface->current.exclusive_zone > 0)
                wlr_scene_layer_surface_v1_configure(surface->scene, &full_area, &usable_area);
        }
    }

    for(auto &layer : { server.layers.shell_background, server.layers.shell_bottom,
                        server.layers.shell_top, server.layers.shell_overlay }) {
        wlr_scene_node *node;
        wl_list_for_each(node, &layer->children, link) {
            layer_shell::LayerSurface *surface =
                static_cast<layer_shell::LayerSurface *>(node->data);
            if(!surface || !surface->layer_surface->initialized)
                continue;
            if(surface->layer_surface->current.exclusive_zone <= 0)
                wlr_scene_layer_surface_v1_configure(surface->scene, &full_area, &usable_area);
        }
    }
}

void output::new_output(wl_listener *listener, void *data) {
    wlr_output *wlr_output = static_cast<struct wlr_output *>(data);
    Output *output = new Output(wlr_output);
    wlr_output->data = output;
    Server::instance().outputs.push_back(output);
}

void output::arrange_outputs() {
    for(auto &output : Server::instance().outputs) output->arrange();
}

void output::frame(wl_listener *listener, void *data) {
    Output *output = static_cast<wrapper::Listener<Output> *>(listener)->container;
    wlr_scene *scene = Server::instance().scene;
    wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->output);

    wlr_scene_output_commit(scene_output, nullptr);

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

void output::request_state(wl_listener *listener, void *data) {
    Output *output = static_cast<wrapper::Listener<Output> *>(listener)->container;
    const wlr_output_event_request_state *event =
        static_cast<wlr_output_event_request_state *>(data);
    wlr_output_commit_state(output->output, event->state);
}

void output::destroy(wl_listener *listener, void *data) {
    Output *output = static_cast<wrapper::Listener<Output> *>(listener)->container;
    Server &server = Server::instance();

    // TODO: Maybe switch to std::list for constant time removal?
    // I don't actually need a vector anyway
    server.outputs.erase(std::remove(server.outputs.begin(), server.outputs.end(), output),
                         server.outputs.end());

    delete output;
}
