/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#pragma once

#include <cstring>
#include <memory>

#include "backend_x64/callback.h"
#include "common/assert.h"
#include "common/cast_util.h"
#include "common/common_types.h"
#include "common/mp.h"

namespace Dynarmic {
namespace BackendX64 {

namespace impl {

template <typename FunctionType, FunctionType mfp>
struct ThunkBuilder;

template <typename C, typename R, typename... Args, R(C::*mfp)(Args...)>
struct ThunkBuilder<R(C::*)(Args...), mfp> {
    static R Thunk(C* this_, Args... args) {
        return (this_->*mfp)(std::forward<Args>(args)...);
    }
};

} // namespace impl

template <typename FunctionType, FunctionType mfp>
ArgCallback DevirtualizeGeneric(mp::class_type_t<FunctionType>* this_) {
    return ArgCallback{&impl::ThunkBuilder<FunctionType, mfp>::Thunk, reinterpret_cast<u64>(this_)};
}

template <typename FunctionType, FunctionType mfp>
ArgCallback DevirtualizeWindows(mp::class_type_t<FunctionType>* this_) {
    static_assert(sizeof(mfp) == 8);
    return ArgCallback{Common::BitCast<u64>(mfp), reinterpret_cast<u64>(this_)};
}

template <typename FunctionType, FunctionType mfp>
ArgCallback DevirtualizeItanium(mp::class_type_t<FunctionType>* this_) {
    struct MemberFunctionPointer {
        /// For a non-virtual function, this is a simple function pointer.
        /// For a virtual function, it is (1 + virtual table offset in bytes).
        u64 ptr;
        /// The required adjustment to `this`, prior to the call.
        u64 adj;
    } mfp_struct = Common::BitCast<MemberFunctionPointer>(mfp);

    static_assert(sizeof(MemberFunctionPointer) == 16);
    static_assert(sizeof(MemberFunctionPointer) == sizeof(mfp));

    u64 fn_ptr = mfp_struct.ptr;
    u64 this_ptr = reinterpret_cast<u64>(this_) + mfp_struct.adj;
    if (mfp_struct.ptr & 1) {
        u64 vtable = Common::BitCastPointee<u64>(this_ptr);
        fn_ptr = Common::BitCastPointee<u64>(vtable + fn_ptr - 1);
    }
    return ArgCallback{fn_ptr, this_ptr};
}

#if defined(__APPLE__) || defined(linux) || defined(__linux) || defined(__linux__)
#define DEVIRT(this_, mfp) Dynarmic::BackendX64::DevirtualizeItanium<decltype(mfp), mfp>(this_)
#elif defined(__MINGW64__)
#define DEVIRT(this_, mfp) Dynarmic::BackendX64::DevirtualizeItanium<decltype(mfp), mfp>(this_)
#elif defined(_WIN32)
#define DEVIRT(this_, mfp) Dynarmic::BackendX64::DevirtualizeWindows<decltype(mfp), mfp>(this_)
#else
#define DEVIRT(this_, mfp) Dynarmic::BackendX64::DevirtualizeGeneric<decltype(mfp), mfp>(this_)
#endif

} // namespace BackendX64
} // namespace Dynarmic
