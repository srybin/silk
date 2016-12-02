#pragma once

#include <initializer_list>

namespace tpf {
    // Forward declaration
    class task;

    namespace parallel {
        static void spawn_one(task& t);

        static void wait_one(task& t);

        static void spawn_all(std::initializer_list<task&> tasks);

        static void wait_all(std::initializer_list<task&> tasks);

        static void wait_any(std::initializer_list<task&> tasks);
    }
}
