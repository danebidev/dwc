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
        : focused_toplevel(nullptr),
          fullscreen(false),
          id(free_workspace_id()),
          active(false),
          output(output) {
        server.root.workspaces[id] = this;
    }

    Workspace::Workspace(int id)
        : focused_toplevel(nullptr),
          fullscreen(false),
          id(id),
          active(false),
          output(server.output_manager.focused_output()) {
        server.root.workspaces[id] = this;
        output->workspaces.push_back(this);
    }

    Workspace::Workspace()
        : focused_toplevel(nullptr),
          fullscreen(false),
          id(free_workspace_id()),
          active(false),
          output(server.output_manager.focused_output()) {
        server.root.workspaces[id] = this;
        output->workspaces.push_back(this);
    }

    void Workspace::switch_focus() {
        if(output == server.output_manager.focused_output() && active)
            return;

        std::pair<double, double> center = output->center();
        server.input_manager.seat.cursor.move_to_coords(center.first, center.second, nullptr);

        assert(output->active_workspace);

        for(auto& toplevel : output->active_workspace->floating)
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);

        for(auto& toplevel : floating)
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);

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
