#include "workspace.hpp"

#include <cassert>
#include <vector>

#include "server.hpp"

namespace workspace {
    int free_workspace_id() {
        int id = 1;
        while(1) {
            if(server.root.workspaces.find(id) == server.root.workspaces.end())
                break;
            id++;
        }

        return id;
    }

    Workspace::Workspace(output::Output* output)
        : output(output),
          fs_scene(wlr_scene_tree_create(server.root.fullscreen)),
          focused_toplevel(nullptr),
          fullscreen(false),
          id(free_workspace_id()),
          active(false) {
        server.root.workspaces[id] = this;
    }

    Workspace::Workspace(int id)
        : output(server.output_manager.focused_output()),
          fs_scene(wlr_scene_tree_create(server.root.fullscreen)),
          focused_toplevel(nullptr),
          fullscreen(false),
          id(id),
          active(false) {
        server.root.workspaces[id] = this;
        output->workspaces.push_back(this);
    }

    Workspace::Workspace()
        : output(server.output_manager.focused_output()),
          fs_scene(wlr_scene_tree_create(server.root.fullscreen)),
          focused_toplevel(nullptr),
          fullscreen(false),
          id(free_workspace_id()),
          active(false) {
        server.root.workspaces[id] = this;
        output->workspaces.push_back(this);
    }

    void Workspace::switch_focus() {
        if(output == server.output_manager.focused_output() && active)
            return;

        // On workspace switch, move the cursor to the center of the output
        std::pair<double, double> center = output->center();
        server.input_manager.seat.cursor.move_to_coords(center.first, center.second, nullptr);

        assert(output->active_workspace);

        // Disable old workspace
        for(auto& toplevel : output->active_workspace->floating)
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
        wlr_scene_node_set_enabled(&output->active_workspace->fs_scene->node, false);

        // Enable new workspace
        for(auto& toplevel : floating)
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
        wlr_scene_node_set_enabled(&output->active_workspace->fs_scene->node, true);

        output->active_workspace->active = false;

        focus();
    }

    void Workspace::focus() {
        if(focused_toplevel)
            server.input_manager.seat.focus_node(&focused_toplevel->node);

        active = true;
        output->active_workspace = this;
    }

    Workspace* focus_or_create(int id) {
        Workspace* ws = nullptr;
        if(server.root.workspaces.find(id) == server.root.workspaces.end())
            ws = new Workspace(id);

        server.root.workspaces[id]->switch_focus();
        return ws;
    }
}
