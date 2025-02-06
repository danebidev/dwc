#include "toplevel.h"

#include <algorithm>
#include <cassert>

#include "server.h"

toplevel::Toplevel* toplevel::desktop_toplevel_at(double lx, double ly, double* sx, double* sy,
                                                  wlr_surface** surface) {
    Server& server = Server::instance();
    wlr_scene_node* node = wlr_scene_node_at(&server.scene->tree.node, lx, ly, sx, sy);
    if(!node || node->type != WLR_SCENE_NODE_BUFFER)
        return nullptr;

    wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);
    wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
    if(!scene_surface)
        return nullptr;

    *surface = scene_surface->surface;

    wlr_scene_tree* tree = node->parent;
    while(tree && tree->node.data) tree = tree->node.parent;

    return static_cast<Toplevel*>(tree->node.data);
}

// Only for keyboard focus
void toplevel::focus_toplevel(Toplevel* toplevel) {
    if(toplevel == nullptr)
        return;

    Server& server = Server::instance();
    wlr_seat* seat = server.seat;
    wlr_surface* prev_surface = seat->keyboard_state.focused_surface;
    wlr_surface* surface = toplevel->toplevel->base->surface;

    if(prev_surface == surface)
        return;

    if(prev_surface) {
        wlr_xdg_toplevel* prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
        if(prev_toplevel)
            wlr_xdg_toplevel_set_activated(prev_toplevel, false);
    }

    wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);
    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
    wlr_xdg_toplevel_set_activated(toplevel->toplevel, true);

    if(keyboard)
        wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes,
                                       &keyboard->modifiers);
}

void toplevel::new_xdg_toplevel(wl_listener* listener, void* data) {
    Server& server = Server::instance();
    wlr_xdg_toplevel* xdg_toplevel = static_cast<wlr_xdg_toplevel*>(data);

    Toplevel* toplevel = new Toplevel();
    toplevel->toplevel = xdg_toplevel;
    // Adds toplevel to scene
    toplevel->scene_tree = wlr_scene_xdg_surface_create(&server.scene->tree, xdg_toplevel->base);
    toplevel->scene_tree->node.data = toplevel;
    xdg_toplevel->base->data = toplevel->scene_tree;

    // wlr_surface listeners
    toplevel->map.notify = xdg_toplevel_map;
    toplevel->unmap.notify = xdg_toplevel_unmap;
    toplevel->commit.notify = xdg_toplevel_commit;
    toplevel->destroy.notify = xdg_toplevel_destroy;

    wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);
    wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

    // xdg_shell listeners
    toplevel->request_move.notify = xdg_toplevel_request_move;
    toplevel->request_resize.notify = xdg_toplevel_request_resize;
    toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
    toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;

    wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
    wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
    wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
}

void toplevel::new_xdg_popup(wl_listener* listener, void* data) {
    wlr_xdg_popup* xdg_popup = static_cast<wlr_xdg_popup*>(data);

    Popup* popup = new Popup;
    popup->popup = xdg_popup;

    wlr_xdg_surface* parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
    assert(parent);
    wlr_scene_tree* parent_tree = static_cast<wlr_scene_tree*>(parent->data);
    xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

    popup->commit.notify = xdg_popup_commit;
    popup->destroy.notify = xdg_popup_destroy;
    wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);
    wl_signal_add(&xdg_popup->base->surface->events.destroy, &popup->destroy);
}

void toplevel::xdg_toplevel_map(wl_listener* listener, void* data) {
    Toplevel* toplevel = wl_container_of(listener, toplevel, map);
    Server::instance().toplevels.push_back(toplevel);
    focus_toplevel(toplevel);
}

void toplevel::xdg_toplevel_unmap(wl_listener* listener, void* data) {
    Server& server = Server::instance();
    Toplevel* toplevel = wl_container_of(listener, toplevel, unmap);

    // Reset cursor mode if the toplevel was currently focused
    if(toplevel == server.grabbed_toplevel)
        cursor::reset_cursor_mode();

    server.toplevels.erase(std::remove(server.toplevels.begin(), server.toplevels.end(), toplevel),
                           server.toplevels.end());
}

void toplevel::xdg_toplevel_commit(wl_listener* listener, void* data) {
    Toplevel* toplevel = wl_container_of(listener, toplevel, commit);

    if(toplevel->toplevel->base->initial_commit) {
        // Set size to 0,0 so the client can choose the size
        wlr_xdg_toplevel_set_size(toplevel->toplevel, 0, 0);
    }
}

void toplevel::xdg_toplevel_destroy(wl_listener* listener, void* data) {
    Toplevel* toplevel = wl_container_of(listener, toplevel, destroy);

    wl_list_remove(&toplevel->map.link);
    wl_list_remove(&toplevel->unmap.link);
    wl_list_remove(&toplevel->commit.link);
    wl_list_remove(&toplevel->destroy.link);

    wl_list_remove(&toplevel->request_move.link);
    wl_list_remove(&toplevel->request_resize.link);
    wl_list_remove(&toplevel->request_maximize.link);
    wl_list_remove(&toplevel->request_fullscreen.link);

    delete toplevel;
}

void toplevel::xdg_toplevel_request_move(wl_listener* listener, void* data) {
    Toplevel* toplevel = wl_container_of(listener, toplevel, request_move);
    Server::instance().begin_interactive(toplevel, cursor::CursorMode::MOVE, 0);
}

void toplevel::xdg_toplevel_request_resize(wl_listener* listener, void* data) {
    wlr_xdg_toplevel_resize_event* event = static_cast<wlr_xdg_toplevel_resize_event*>(data);
    Toplevel* toplevel = wl_container_of(listener, toplevel, request_resize);
    Server::instance().begin_interactive(toplevel, cursor::CursorMode::RESIZE, event->edges);
}

void toplevel::xdg_toplevel_request_maximize(wl_listener* listener, void* data) {
    Toplevel* toplevel = wl_container_of(listener, toplevel, request_maximize);
    if(toplevel->toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel->toplevel->base);
}

void toplevel::xdg_toplevel_request_fullscreen(wl_listener* listener, void* data) {
    Toplevel* toplevel = wl_container_of(listener, toplevel, request_fullscreen);
    if(toplevel->toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel->toplevel->base);
}

void toplevel::xdg_popup_commit(wl_listener* listener, void* data) {
    Popup* popup = wl_container_of(listener, popup, commit);

    if(popup->popup->base->initial_commit)
        wlr_xdg_surface_schedule_configure(popup->popup->base);
}

void toplevel::xdg_popup_destroy(wl_listener* listener, void* data) {
    Popup* popup = wl_container_of(listener, popup, commit);

    wl_list_remove(&popup->commit.link);
    wl_list_remove(&popup->destroy.link);

    delete popup;
}
