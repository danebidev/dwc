#pragma once

#include "output.hpp"
#include "vector"
#include "wlr.hpp"
#include "xdg-shell.hpp"

namespace layer_shell {
    void new_surface(Server* server, void* data);

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

        wrapper::Listener<LayerSurface> map_list;
        wrapper::Listener<LayerSurface> unmap_list;
        wrapper::Listener<LayerSurface> commit_list;
        wrapper::Listener<LayerSurface> output_destroy_list;
        wrapper::Listener<LayerSurface> destroy_list;
        wrapper::Listener<LayerSurface> new_popup_list;

        void map(LayerSurface* surface, void* data);
        void unmap(LayerSurface* surface, void* data);
        void commit(LayerSurface* surface, void* data);
        void output_destroy(LayerSurface* surface, void* data);
        void destroy(LayerSurface* surface, void* data);
        void new_popup(LayerSurface* surface, void* data);
    };
}
