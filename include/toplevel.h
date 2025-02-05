#pragma once

#include "wlr.h"

namespace toplevel {
    struct Toplevel {
        wlr_xdg_toplevel* toplevel;
        wlr_scene_tree* scene_tree;

        wl_listener map;
        wl_listener unmap;
        wl_listener commit;
        wl_listener destroy;

        wl_listener request_move;
        /*wl_listener request_resize;*/
        /*wl_listener request_maximize;*/
        /*wl_listener request_fullscreen;*/
    };

    struct Popup {
        wlr_xdg_popup* popup;

        wl_listener commit;
        wl_listener destroy;
    };

    Toplevel* desktop_toplevel_at(double lx, double ly, double* sx, double* sy,
                                  wlr_surface** surface);

    // Called when a surface is created by a client
    void new_xdg_toplevel(wl_listener* listener, void* data);

    // Called when a surface is created by a client
    void new_xdg_popup(wl_listener* listener, void* data);

    // Called when a surface gets mapped
    void xdg_toplevel_map(wl_listener* listener, void* data);

    // Called when an xdg_toplevel gets unmapped
    void xdg_toplevel_unmap(wl_listener* listener, void* data);

    // Called when an xdg_toplevel gets destroyed
    void xdg_toplevel_commit(wl_listener* listener, void* data);

    // Called when an xdg_toplevel gets mapped
    void xdg_toplevel_destroy(wl_listener* listener, void* data);

    // Called when an xdg_toplevel gets mapped
    void xdg_toplevel_request_move(wl_listener* listener, void* data);

    void xdg_popup_commit(wl_listener* listener, void* data);
    void xdg_popup_destroy(wl_listener* listener, void* data);
}
