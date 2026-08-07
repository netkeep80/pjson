#include <pjson/pjson.h>

#include <type_traits>

// @req CR-001 — pjson compiles and runs directly against PMM manager types.
// @req NFR-001 — a consumer needs only headers and a C++20 compiler.
int main()
{
    using DefaultManager = pjson::default_manager;
    static_assert(std::is_same_v<DefaultManager, pjson::manager<pmm::CacheManagerConfig, 0>>);

    if (!DefaultManager::create(64 * 1024))
        return 1;

    auto value = DefaultManager::create_typed<int>(42);
    if (!value || *value != 42)
    {
        DefaultManager::destroy();
        return 2;
    }

    DefaultManager::destroy_typed(value);
    DefaultManager::destroy();

    using ManagerA = pjson::single_threaded_manager<1>;
    using ManagerB = pjson::single_threaded_manager<2>;
    static_assert(!std::is_same_v<ManagerA, ManagerB>);

    if (!ManagerA::create(64 * 1024))
        return 3;
    if (!ManagerB::create(64 * 1024))
    {
        ManagerA::destroy();
        return 4;
    }

    auto a = ManagerA::create_typed<int>(11);
    auto b = ManagerB::create_typed<int>(22);
    if (!a || !b || *a != 11 || *b != 22)
    {
        ManagerA::destroy();
        ManagerB::destroy();
        return 5;
    }

    // Destroying one PMM specialization must not affect another. pjson uses
    // the manager type as its state boundary instead of a process-wide singleton.
    ManagerA::destroy_typed(a);
    ManagerA::destroy();

    if (ManagerA::is_initialized() || !ManagerB::is_initialized() || *b != 22)
    {
        ManagerB::destroy();
        return 6;
    }

    ManagerB::destroy_typed(b);
    ManagerB::destroy();
    return 0;
}
