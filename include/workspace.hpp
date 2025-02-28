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
        output::Output* output;
        std::list<xdg_shell::Toplevel*> floating;
        wlr_scene_tree* fs_scene;

        xdg_shell::Toplevel* focused_toplevel;
        bool fullscreen;

        Workspace(output::Output* output);
        Workspace(int id);
        Workspace();

        // Handles workspace focus when there's a workspace switch, so
        // when a workspace is hidden to show another one instead on the same output
        void switch_focus();
        // Generic workspace focus, switches the focus to the new workspace and focuses a toplevel
        // Called when a new workspace so focused for any reason, like
        // the 'workspace' command or a cursor moving between outputs
        void focus();

        private:
        int id;
        bool active;
    };

    Workspace* focus_or_create(int id);
}
