#include "wlr-wrapper.hpp"

#include <cassert>

#include "server.hpp"

namespace wrapper {
    template <typename T>
    void handle(wl_listener* listener, void* data) {
        // This is extremely hacky, but it's the best way I could find
        Listener<T>* list = (Listener<T>*)listener;
        list->emit(data);
    }

    template <typename T>
    Listener<T>::Listener(T* container, wl_signal* signal, const Callback& callback)
        : connected(false) {
        wl_list_init(&listener.link);
        init(container, signal, callback);
    }

    template <typename T>
    Listener<T>::~Listener() {
        free();
    }

    template <typename T>
    void Listener<T>::init(T* cont, wl_signal* signal, const Callback& cb) {
        assert(!connected);

        container = cont;
        callback = cb;
        listener.notify = handle<T>;

        wl_signal_add(signal, &listener);
        connected = true;
    }

    template <typename T>
    void Listener<T>::free() {
        if(connected) {
            wl_list_remove(&listener.link);
            connected = false;
        }
    }

    template <typename T>
    void Listener<T>::emit(void* data) {
        callback(container, data);
    }

    template struct Listener<Server>;

    template struct Listener<xdg_shell::Toplevel>;
    template struct Listener<xdg_shell::Popup>;
    template struct Listener<layer_shell::LayerSurface>;

    template struct Listener<output::OutputManager>;
    template struct Listener<output::Output>;

    template struct Listener<input::InputManager>;
    template struct Listener<input::InputDevice>;
    template struct Listener<cursor::Cursor>;
    template struct Listener<keyboard::Keyboard>;
    template struct Listener<seat::SeatNode>;
    template struct Listener<seat::Seat>;
}
