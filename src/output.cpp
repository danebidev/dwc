#include "output.h"

#include <algorithm>

#include "server.h"

// Raised when a new output (monitor or display) becomes available
// data is a wlr_output.
void output::new_output(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_output *new_output = (wlr_output *)data;

    // Configures the output to use our allocator and renderer
    wlr_output_init_render(new_output, server.allocator, server.renderer);

    // Enables the output if it's not enabled
    wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    // TODO: config
    wlr_output_mode *mode = wlr_output_preferred_mode(new_output);
    if(mode)
        wlr_output_state_set_mode(&state, mode);

    // Applies the state
    wlr_output_commit_state(new_output, &state);
    wlr_output_state_finish(&state);

    Output *output = new Output();
    output->output = new_output;

    // Listeners for output events
    output->frame.notify = frame;
    output->request_state.notify = request_state;
    output->destroy.notify = destroy;

    wl_signal_add(&new_output->events.frame, &output->frame);
    wl_signal_add(&new_output->events.request_state, &output->request_state);
    wl_signal_add(&new_output->events.destroy, &output->destroy);

    server.outputs.push_back(output);

    wlr_output_layout_output *layout_output =
        wlr_output_layout_add_auto(server.output_layout, new_output);
    wlr_scene_output *scene_output = wlr_scene_output_create(server.scene, new_output);

    wlr_scene_output_layout_add_output(server.scene_layout, layout_output, scene_output);
}

// Called whenever an output wants to display a frame.
// Generally should be at the output's refresh rate
void output::frame(wl_listener *listener, void *data) {
    Output *output = wl_container_of(listener, output, frame);
    wlr_scene *scene = Server::instance().scene;
    wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->output);

    wlr_scene_output_commit(scene_output, NULL);

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

// Called when the backend request a new state.
// For example, resizing a window in the X11 or wayland backend
void output::request_state(wl_listener *listener, void *data) {
    Output *output = wl_container_of(listener, output, request_state);
    const wlr_output_event_request_state *event = (wlr_output_event_request_state *)data;
    wlr_output_commit_state(output->output, event->state);
}

// Called when an output is destroyed.
void output::destroy(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    Output *output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);

    // TODO: Maybe switch to std::list for constant time removal?
    server.outputs.erase(std::remove(server.outputs.begin(), server.outputs.end(), output),
                         server.outputs.end());

    delete output;
}
