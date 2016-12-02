#pragma once

namespace tpf {
    class no_copy {
    public:
        no_copy(no_copy&) = delete;
    };

    class no_assign : public no_copy {
    public:
        no_assign& operator = (const no_copy&) = delete;
    };

    //! Basic padding with template selection
    template<typename TClass, size_t Size, size_t CacheLineSize = 128>
    struct cache_padding_base : TClass {
        uint8_t pad[CacheLineSize - sizeof( TClass ) % CacheLineSize];
    };

    //! Basic padding with default "0" selection parameter
    template<typename TClass> struct cache_padding_base< TClass, 0 > : TClass {};

    template<typename TClass, size_t CacheLineSize>
    struct cache_padding_base<TClass, 0, CacheLineSize> : TClass {};

    //! Pads type T to fill out to a multiple of cache line size.
    template<typename TClass> struct cache_padding : cache_padding_base< TClass, sizeof( TClass ) > {};

    template<typename TClass, size_t CacheLineSize>
    struct cache_padding<TClass, 0, CacheLineSize> : cache_padding_base<TClass, sizeof( TClass ), CacheLineSize> {};
}