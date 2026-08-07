#pragma once

// @req CR-001 — PMM is the persistent-memory and persistent-container foundation.
// @req NFR-001 — pjson exposes a header-only C++20 include surface.
#include "pjson/manager.h"

namespace pjson
{

// JSON node semantics are introduced in the next phase. This umbrella header
// currently exposes only the standalone build and manager/configuration
// boundary established by issues #28 and #29.

} // namespace pjson
