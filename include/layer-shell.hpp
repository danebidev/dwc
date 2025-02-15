#pragma once

#include "output.hpp"
#include "vector"
#include "wlr.hpp"
#include "xdg-shell.hpp"

namespace layer_shell {
    void new_surface(wl_listener* listener, void* data);

    class LayerSurface {
        public:
        wlr_layer_surface_v1* layer_surface;
        nodes::Node node;

        wlr_scene_layer_surface_v1* scene;
        output::Output* output;

        std::vector<xdg_shell::Popup*> popups;

        LayerSurface(wlr_scene_layer_surface_v1* layer_surface, output::Output* output);

        void handle_focus();

        private:
        wlr_scene_tree* popup_tree;
        wlr_scene_tree* tree;

        wrapper::Listener<LayerSurface> map;
        wrapper::Listener<LayerSurface> unmap;
        wrapper::Listener<LayerSurface> surface_commit;
        wrapper::Listener<LayerSurface> output_destroy;
        wrapper::Listener<LayerSurface> node_destroy;
        wrapper::Listener<LayerSurface> new_popup;
    };
}
