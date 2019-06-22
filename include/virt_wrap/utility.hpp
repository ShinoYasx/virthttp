//
// Created by _as on 2019-01-31.
//

#pragma once

#include <algorithm>
#include <cstdlib>
#include <type_traits>

template <typename T> using passive = T;

template <typename T> inline void freeany(T ptr) {
    static_assert(std::is_array_v<T> || std::is_pointer_v<T>, "T needs to be a pointer or an array type");
    std::free(ptr);
}

template <typename T> struct NoallocWFree : private std::allocator<gsl::owner<T>> { // This clearly needs to be shadowed from the library users
    using value_type = gsl::owner<T>;
    using std::allocator<gsl::owner<T>>::allocate;
    using std::allocator<gsl::owner<T>>::deallocate;
    constexpr inline void construct(gsl::owner<T>* ptr) const noexcept {}
    inline void destroy(gsl::owner<T>* ptr) const noexcept { freeany(*ptr); }
};

namespace virt::meta {
namespace light {
template <typename U, typename CountFRet, typename DataFRet, typename T>
auto wrap_oparm_owning_fill_freeable_arr(U underlying, CountFRet (*count_fcn)(U), DataFRet (*data_fcn)(U, T*, CountFRet)) {
    using LocAlloc = NoallocWFree<T>;
    using RetType = std::optional<std::vector<T, LocAlloc>>;
    std::vector<gsl::owner<T>, LocAlloc> ret{};
    ret.resize(count_fcn(underlying));
    if (!ret.empty()) {
        const auto res = data_fcn(underlying, ret.data(), ret.size());
        if (res != 0)
            return RetType{std::nullopt};
    }
    return RetType{ret};
}

template <typename Wrap, typename U, typename DataFRet, typename T, typename... DataFArgs>
auto wrap_opram_owning_set_autodestroyable_arr(U underlying, DataFRet (*data_fcn)(U, T**, DataFArgs...),
                                               DataFArgs... data_f_args) { // This one can shadow
    using RetType = std::optional<std::unique_ptr<Wrap[], void (*)(Wrap*)>>;
    Wrap* lease_arr;
    auto res = data_fcn(underlying, reinterpret_cast<T**>(&lease_arr), data_f_args...);
    if (res == -1)
        return RetType{std::nullopt};
    return std::optional{std::unique_ptr<Wrap[], void (*)(Wrap*)>(lease_arr, [](auto arr) {
        auto it = arr;
        while (it++)
            it->~Wrap();
        freeany(arr);
    })};
}

template <typename Wrap, void (*dtroy)(Wrap*) = std::destroy_at<Wrap>, typename U, typename DataFRet, typename T, typename... DataFArgs>
auto wrap_opram_owning_set_destroyable_arr(U underlying, DataFRet (*data_fcn)(U, T**, DataFArgs...), DataFArgs... data_f_args) {
    if constexpr (dtroy == std::destroy_at<Wrap>) {
        return wrap_opram_owning_set_autodestroyable_arr<Wrap>(underlying, data_fcn, data_f_args...);
    }
    using RetType = std::optional<std::unique_ptr<Wrap[], void (*)(Wrap*)>>;
    Wrap* lease_arr;
    auto res = data_fcn(underlying, reinterpret_cast<T**>(&lease_arr), data_f_args...);
    if (res == -1)
        return RetType{std::nullopt};
    return std::optional{std::unique_ptr<Wrap[], void (*)(Wrap*)>(lease_arr, [](Wrap* arr) {
        auto it = arr;
        while (it++)
            dtroy(it);
        freeany(arr);
    })};
}
} // namespace light
namespace heavy {} // namespace heavy
} // namespace virt::meta

// From C++ Weekly - Ep 134 (Jason Turner) :
// https://www.youtube.com/watch?v=EsUmnLgz8QY

template <typename... Base> struct Visitor : Base... { using Base::operator()...; };

template <typename... T> Visitor(T...)->Visitor<T...>;