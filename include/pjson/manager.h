#pragma once

#include <cstddef>

#include "pmm/manager_configs.h"
#include "pmm/persist_memory_manager.h"

namespace pjson
{

// @req CR-001 — pjson uses PMM directly instead of introducing a second
// persistent-memory manager or compatibility adapter.
template <typename Config = pmm::CacheManagerConfig, std::size_t InstanceId = 0>
using manager = pmm::PersistMemoryManager<Config, InstanceId>;

// The default pjson manager favors the lightweight single-threaded PMM config.
// Applications that need a different storage/locking policy can inject any PMM
// Config through pjson::manager without changing pjson's persistent model.
template <std::size_t InstanceId = 0>
using single_threaded_manager = manager<pmm::CacheManagerConfig, InstanceId>;

template <std::size_t InstanceId = 0>
using multi_threaded_manager = manager<pmm::PersistentDataConfig, InstanceId>;

using default_manager = single_threaded_manager<0>;

} // namespace pjson
