#pragma once

#include <string>

#include "wlr.hpp"
#include "workspace.hpp"
#include "xdg-shell.hpp"

namespace output {
    class Output;
}

namespace workspace {
    class Workspace {
        public:
        std::list<xdg_shell::Toplevel*> floating;
        xdg_shell::Toplevel* last_focused_toplevel;

        Workspace(output::Output* output);
        Workspace(int id);
        Workspace();

        void focus();

        private:
        int id;
        bool active;
        output::Output* output;
    };

    int free_workspace_id();
    Workspace* focus_or_create(int id);
}
