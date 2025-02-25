#pragma once

#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"

namespace xdg_shell {
    // Called when a new toplevel is created by a client
    void new_xdg_toplevel(wl_listener* listener, void* data);

    class Toplevel {
        public:
        wlr_xdg_toplevel* toplevel;
        nodes::Node node;

        wlr_scene_tree* scene_tree;
        workspace::Workspace* workspace;

        Toplevel(wlr_xdg_toplevel* toplevel);

        output::Output* output();
        // Toggles fullscreen status
        void fullscreen();
        // Sets size and position of a fullscreened toplevel
        void update_fullscreen();

        private:
        // To restore the original geometry on exit fullscreen
        wlr_box saved_geometry;

        wrapper::Listener<Toplevel> map;
        wrapper::Listener<Toplevel> unmap;
        wrapper::Listener<Toplevel> commit;
        wrapper::Listener<Toplevel> destroy;

        wrapper::Listener<Toplevel> request_move;
        wrapper::Listener<Toplevel> request_resize;
        wrapper::Listener<Toplevel> request_maximize;
        wrapper::Listener<Toplevel> request_minimize;
        wrapper::Listener<Toplevel> request_fullscreen;

        wrapper::Listener<Toplevel> new_popup;
    };

    class Popup {
        public:
        wlr_xdg_popup* popup;
        wlr_scene_tree* scene;

        Popup(wlr_xdg_popup* xdg_popup, wlr_scene_tree* popup);

        private:
        wrapper::Listener<Popup> commit;
        wrapper::Listener<Popup> destroy;
        wrapper::Listener<Popup> new_popup;
    };
}
