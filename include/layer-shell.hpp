#pragma once

#include "output.hpp"
#include "vector"
#include "wlr.hpp"
#include "xdg-shell.hpp"

namespace layer_shell {
    struct LayerSurface {
        wlr_layer_surface_v1* layer_surface;
        wlr_scene_layer_surface_v1* scene;
        nodes::Node node;

        wlr_scene_tree* popup_tree;
        wlr_scene_tree* tree;

        output::Output* output;
        std::vector<xdg_shell::Popup*> popups;

        wrapper::Listener<LayerSurface> map;
        wrapper::Listener<LayerSurface> unmap;
        wrapper::Listener<LayerSurface> surface_commit;
        wrapper::Listener<LayerSurface> output_destroy;
        wrapper::Listener<LayerSurface> node_destroy;
        wrapper::Listener<LayerSurface> new_popup;

        bool mapped;

        LayerSurface(wlr_scene_layer_surface_v1* layer_surface, output::Output* output);

        void handle_focus();
    };

    void new_surface(wl_listener* listener, void* data);

    // misc.
    wlr_scene_tree* get_scene(output::Output* output, zwlr_layer_shell_v1_layer type);

    // Listeners
    void map(wl_listener* listener, void* data);
    void unmap(wl_listener* listener, void* data);
    void surface_commit(wl_listener* listener, void* data);
    void output_destroy(wl_listener* listener, void* data);
    void destroy(wl_listener* listener, void* data);
    void new_popup(wl_listener* listener, void* data);
}
