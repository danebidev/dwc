#pragma once

#include <wayland-server-core.h>

#include "wlr.hpp"

namespace wrapper {
    template <typename Container>
    class Listener : public wl_listener {
        public:
        using Callback = void(wl_listener*, void*);

        Listener(Container* cont, Callback cb, wl_signal* signal)
            : freed(false) {
            container = cont;
            notify = cb;
            wl_signal_add(signal, this);
        }

        ~Listener() { free(); }

        // Needed because listeners sometimes have to be deleted manually
        void free() {
            if(!freed) {
                wl_list_remove(&link);
                freed = true;
            }
        }

        Container* container;

        private:
        bool freed;
    };
}
