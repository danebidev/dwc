#pragma once

#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"

namespace xdg_shell {
    struct Toplevel {
        wlr_xdg_toplevel* toplevel;
        wlr_scene_tree* scene_tree;
        nodes::Node node;

        wrapper::Listener<Toplevel> map;
        wrapper::Listener<Toplevel> unmap;
        wrapper::Listener<Toplevel> commit;
        wrapper::Listener<Toplevel> destroy;

        wrapper::Listener<Toplevel> request_move;
        wrapper::Listener<Toplevel> request_resize;
        wrapper::Listener<Toplevel> request_maximize;
        wrapper::Listener<Toplevel> request_fullscreen;

        wrapper::Listener<Toplevel> new_popup;

        Toplevel(wlr_xdg_toplevel* toplevel);
    };

    struct Popup {
        wlr_xdg_popup* popup;

        wlr_scene_tree* scene;

        wrapper::Listener<Popup> commit;
        wrapper::Listener<Popup> destroy;
        wrapper::Listener<Popup> new_popup;

        Popup(wlr_xdg_popup* xdg_popup, wlr_scene_tree* popup);
    };

    // Called when a surface is created by a client
    void new_xdg_toplevel(wl_listener* listener, void* data);

    // Called when a surface gets mapped
    void xdg_toplevel_map(wl_listener* listener, void* data);

    // Called when an xdg_toplevel gets unmapped
    void xdg_toplevel_unmap(wl_listener* listener, void* data);

    // Called when a commit gets applied to a toplevel
    void xdg_toplevel_commit(wl_listener* listener, void* data);

    // Called when an xdg_toplevel gets destroyed
    void xdg_toplevel_destroy(wl_listener* listener, void* data);

    // Called when an xdg_toplevel requests a move
    void xdg_toplevel_request_move(wl_listener* listener, void* data);

    // Called when an xdg_toplevel requests a resize
    void xdg_toplevel_request_resize(wl_listener* listener, void* data);

    // Called when an xdg_toplevel requests to be maximized
    void xdg_toplevel_request_maximize(wl_listener* listener, void* data);

    // Called when an xdg_toplevel requests fullscreen
    void xdg_toplevel_request_fullscreen(wl_listener* listener, void* data);

    // Called when a popup is created by a client
    void xdg_toplevel_new_popup(wl_listener* listener, void* data);

    void xdg_popup_commit(wl_listener* listener, void* data);
    void xdg_popup_destroy(wl_listener* listener, void* data);
    void xdg_popup_new_popup(wl_listener* listener, void* data);
}
