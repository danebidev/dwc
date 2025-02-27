#pragma once

#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"

class Server;

namespace xdg_shell {
    // Called when a new toplevel is created by a client
    void new_xdg_toplevel(Server* server, void* data);

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

        wrapper::Listener<Toplevel> map_list;
        wrapper::Listener<Toplevel> unmap_list;
        wrapper::Listener<Toplevel> commit_list;
        wrapper::Listener<Toplevel> destroy_list;

        wrapper::Listener<Toplevel> new_popup_list;

        wrapper::Listener<Toplevel> request_move_list;
        wrapper::Listener<Toplevel> request_resize_list;
        wrapper::Listener<Toplevel> request_maximize_list;
        wrapper::Listener<Toplevel> request_minimize_list;
        wrapper::Listener<Toplevel> request_fullscreen_list;

        // Called when a surface gets mapped
        void map(Toplevel* toplevel, void* data);
        // Called when an xdg_toplevel gets unmapped
        void unmap(Toplevel* toplevel, void* data);
        // Called when a commit gets applied to a toplevel
        void commit(Toplevel* toplevel, void* data);
        // Called when an xdg_toplevel gets destroyed
        void destroy(Toplevel* toplevel, void* data);

        // Called when a popup is created by a client
        void new_popup(Toplevel* toplevel, void* data);

        // Called when an xdg_toplevel requests a move
        void request_move(Toplevel* toplevel, void* data);
        // Called when an xdg_toplevel requests a resize
        void request_resize(Toplevel* toplevel, void* data);
        // Called when an xdg_toplevel requests to be maximized
        void request_maximize(Toplevel* toplevel, void* data);
        // Called when an xdg_toplevel requests to be minimized
        void request_minimize(Toplevel* toplevel, void* data);
        // Called when an xdg_toplevel requests fullscreen
        void request_fullscreen(Toplevel* toplevel, void* data);
    };

    class Popup {
        public:
        wlr_xdg_popup* popup;
        wlr_scene_tree* scene;

        Popup(wlr_xdg_popup* xdg_popup, wlr_scene_tree* popup);

        private:
        wrapper::Listener<Popup> commit_list;
        wrapper::Listener<Popup> destroy_list;
        wrapper::Listener<Popup> new_popup_list;

        void commit(Popup* popup, void* data);
        void destroy(Popup* popup, void* data);
        void new_popup(Popup* popup, void* data);
    };
}
