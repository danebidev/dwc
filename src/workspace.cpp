#include "workspace.hpp"

#include <vector>

#include "server.hpp"

namespace workspace {
    Workspace::Workspace(output::Output* output)
        : id(free_workspace_id()),
          active(false),
          output(output) {
        server.root.workspaces[id] = this;
    }

    Workspace::Workspace(int id)
        : id(id),
          active(false),
          output(server.output_manager.focused_output()) {
        server.root.workspaces[id] = this;
        output->workspaces.push_back(this);
    }

    Workspace::Workspace()
        : id(free_workspace_id()),
          active(false),
          output(server.output_manager.focused_output()) {
        server.root.workspaces[id] = this;
        output->workspaces.push_back(this);
    }

    void Workspace::focus() {
        if(output == server.output_manager.focused_output() && active)
            return;

        std::pair<double, double> center = output->center();
        wlr_input_device* device = nullptr;
        for(auto& dev : server.input_manager.devices) {
            if(dev->device->type == WLR_INPUT_DEVICE_POINTER)
                device = dev->device;
        }
        server.input_manager.seat.cursor.move_to_coords(center.first, center.second, device);

        if(active)
            return;

        if(output->active_workspace && output->active_workspace != this) {
            for(auto& toplevel : output->active_workspace->floating)
                wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
        }

        for(auto& toplevel : floating)
            wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);

        if(output->active_workspace) {
            output->active_workspace->active = false;
        }

        active = true;
        output->active_workspace = this;
    }

    int free_workspace_id() {
        int id = 1;
        while(1) {
            if(server.root.workspaces.find(id) == server.root.workspaces.end())
                break;
            id++;
        }

        return id;
    }

    Workspace* focus_or_create(int id) {
        Workspace* ws = nullptr;
        if(server.root.workspaces.find(id) == server.root.workspaces.end())
            ws = new Workspace(id);

        server.root.workspaces[id]->focus();
        return ws;
    }
}
