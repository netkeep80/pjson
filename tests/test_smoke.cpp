#include <pjson/pjson.h>

// @req CR-001 — pjson compiles and runs against the pinned PMM dependency.
// @req NFR-001 — a consumer needs only headers and a C++20 compiler.
int main()
{
    using Manager = pmm::presets::SingleThreadedHeap;

    if (!Manager::create(64 * 1024))
        return 1;

    auto value = Manager::create_typed<int>(42);
    if (!value)
    {
        Manager::destroy();
        return 2;
    }

    if (*value != 42)
    {
        Manager::destroy_typed(value);
        Manager::destroy();
        return 3;
    }

    *value = 100;
    if (*value != 100)
    {
        Manager::destroy_typed(value);
        Manager::destroy();
        return 4;
    }

    Manager::destroy_typed(value);
    Manager::destroy();
    return 0;
}
