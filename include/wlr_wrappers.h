#pragma once

#include <wayland-server-core.h>

#include "wlr.h"

/*namespace wrapper {*/
/*    template <typename Container>*/
/*    class Listener : wl_listener {*/
/*        public:*/
/*        using Callback = void(wl_listener*, void*);*/
/**/
/*        Listener(Container cont, Callback callback) {*/
/*            container = cont;*/
/*            notify = callback;*/
/*        }*/
/**/
/*        void registerEvent(wl_signal* signal) { wl_signal_add(signal, this); }*/
/**/
/*        private:*/
/*        Container container;*/
/*    };*/
/*}*/
