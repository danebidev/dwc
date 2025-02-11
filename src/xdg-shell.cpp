#include "xdg-shell.hpp"

#include <algorithm>
#include <cassert>

#include "server.hpp"

xdg_shell::Toplevel::Toplevel(wlr_xdg_toplevel* xdg_toplevel)
    : toplevel(xdg_toplevel),
      scene_tree(wlr_scene_xdg_surface_create(Server::instance().layers.floating, toplevel->base)),

      map(this, xdg_toplevel_map, &xdg_toplevel->base->surface->events.map),
      unmap(this, xdg_toplevel_unmap, &xdg_toplevel->base->surface->events.unmap),
      commit(this, xdg_toplevel_commit, &xdg_toplevel->base->surface->events.commit),
      destroy(this, xdg_toplevel_destroy, &xdg_toplevel->events.destroy),

      request_move(this, xdg_toplevel_request_move, &xdg_toplevel->events.request_move),
      request_resize(this, xdg_toplevel_request_resize, &xdg_toplevel->events.request_resize),
      request_maximize(this, xdg_toplevel_request_maximize, &xdg_toplevel->events.request_maximize),
      request_fullscreen(this, xdg_toplevel_request_fullscreen,
                         &xdg_toplevel->events.request_fullscreen) {
    toplevel->base->data = scene_tree;
    scene_tree->node.data = this;
}

xdg_shell::Popup::Popup(wlr_xdg_popup* xdg_popup, wlr_scene_tree* parent)
    : popup(xdg_popup),
      commit(this, xdg_popup_commit, &xdg_popup->base->surface->events.commit),
      destroy(this, xdg_popup_destroy, &xdg_popup->events.destroy) {
    xdg_popup->base->data = wlr_scene_xdg_surface_create(parent, xdg_popup->base);
}

// Only for keyboard focus
void xdg_shell::focus_toplevel(Toplevel* toplevel) {
    if(toplevel == nullptr)
        return;

    Server& server = Server::instance();
    wlr_seat* seat = server.seat.seat;
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

void xdg_shell::new_xdg_toplevel(wl_listener* listener, void* data) {
    wlr_xdg_toplevel* xdg_toplevel = static_cast<wlr_xdg_toplevel*>(data);

    new Toplevel(xdg_toplevel);
}

void xdg_shell::new_xdg_popup(wl_listener* listener, void* data) {
    wlr_xdg_popup* xdg_popup = static_cast<wlr_xdg_popup*>(data);
    wlr_xdg_surface* parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
    wlr_scene_tree* parent_tree = static_cast<wlr_scene_tree*>(parent->data);
    new Popup(xdg_popup, parent_tree);
}

void xdg_shell::xdg_toplevel_map(wl_listener* listener, void* data) {
    Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
    Server::instance().toplevels.push_back(toplevel);
    focus_toplevel(toplevel);
}

void xdg_shell::xdg_toplevel_unmap(wl_listener* listener, void* data) {
    Server& server = Server::instance();
    Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;

    // Reset cursor mode if the toplevel was currently focused
    if(toplevel == server.cursor.grabbed_toplevel)
        server.cursor.reset_cursor_mode();

    server.toplevels.erase(std::remove(server.toplevels.begin(), server.toplevels.end(), toplevel),
                           server.toplevels.end());
}

void xdg_shell::xdg_toplevel_commit(wl_listener* listener, void* data) {
    Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;

    if(toplevel->toplevel->base->initial_commit) {
        // Set size to 0,0 so the client can choose the size
        wlr_xdg_toplevel_set_size(toplevel->toplevel, 0, 0);
    }
}

void xdg_shell::xdg_toplevel_destroy(wl_listener* listener, void* data) {
    Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
    delete toplevel;
}

void xdg_shell::xdg_toplevel_request_move(wl_listener* listener, void* data) {
    Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
    Server::instance().cursor.begin_interactive(toplevel, cursor::CursorMode::MOVE, 0);
}

void xdg_shell::xdg_toplevel_request_resize(wl_listener* listener, void* data) {
    Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
    wlr_xdg_toplevel_resize_event* event = static_cast<wlr_xdg_toplevel_resize_event*>(data);
    Server::instance().cursor.begin_interactive(toplevel, cursor::CursorMode::RESIZE, event->edges);
}

void xdg_shell::xdg_toplevel_request_maximize(wl_listener* listener, void* data) {
    Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
    if(toplevel->toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel->toplevel->base);
}

void xdg_shell::xdg_toplevel_request_fullscreen(wl_listener* listener, void* data) {
    Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
    if(toplevel->toplevel->base->initialized)
        wlr_xdg_surface_schedule_configure(toplevel->toplevel->base);
}

void xdg_shell::xdg_popup_commit(wl_listener* listener, void* data) {
    Popup* popup = static_cast<wrapper::Listener<Popup>*>(listener)->container;

    if(popup->popup->base->initial_commit)
        wlr_xdg_surface_schedule_configure(popup->popup->base);
}

void xdg_shell::xdg_popup_destroy(wl_listener* listener, void* data) {
    delete static_cast<wrapper::Listener<Popup>*>(listener)->container;
}
