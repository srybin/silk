#pragma once

#include <tpf/task.h>

namespace tpf {
    template<typename R, typename U> class functional_task;

    template<typename ReturnType, typename Args...>
    class functional_task<ReturnType, ReturnType(Args...)> : public task {
    public:
    };
}
