#pragma once

// IWYU pragma: begin_exports
#include <wayland-server-core.h>

extern "C" {
#define WLR_USE_UNSTABLE 1
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
}
// IWYU pragma: end_exports
