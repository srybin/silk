#pragma once

#include <type_traits>

namespace tpf {
    /// Size of pointer in current compilator and OS
    struct size_of_ptr_trait { enum { value = sizeof( uintptr_t ) }; };
}
