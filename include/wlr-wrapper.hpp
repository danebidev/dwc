#pragma once

#include <wayland-server-core.h>

#include <functional>

#include "wlr.hpp"

namespace wrapper {
    // This whole thing is hacky af
    template <typename Container>
    struct Listener {
        // Has to remaint first
        wl_listener listener;

        using Callback = std::function<void(Container*, void*)>;

        template <typename T>
        friend void handle(wl_listener* listener, void* data);

        public:
        Listener(Container* container, wl_signal* signal, const Callback& callback);
        ~Listener();

        void init(Container* cont, wl_signal* signal, const Callback& cb);
        void free();

        private:
        bool connected();
        void emit(void* data);

        Container* container;
        Callback callback;
    };
}
