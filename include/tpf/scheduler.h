#pragma once

namespace tpf {
    // Forward declarations
    class task;

    class scheduler {
    public:
        virtual ~scheduler() {}

        virtual void spawn(std::initializer_list<task *> t) = 0;

        virtual void wait(std::initializer_list<task *> t) = 0;
    };
}