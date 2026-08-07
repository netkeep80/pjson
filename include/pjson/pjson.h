#pragma once

// @req CR-001 — PMM is the persistent-memory and persistent-container foundation.
// @req NFR-001 — pjson exposes a header-only C++20 include surface.
#include "pmm/pmm_presets.h"

namespace pjson
{

// The first standalone pjson surface intentionally contains no JSON node
// semantics yet. Issue #28 establishes only the public include/build boundary;
// the manager/configuration contract is introduced separately in issue #29.

} // namespace pjson
