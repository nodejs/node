//===-- Shared memory RPC client / server utilities -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_RPC_UTIL_H
#define LLVM_LIBC_SHARED_RPC_UTIL_H

#include <stddef.h>
#include <stdint.h>

#if (defined(__NVPTX__) || defined(__AMDGPU__) || defined(__SPIRV__)) &&       \
    !((defined(__CUDA__) && !defined(__CUDA_ARCH__)) ||                        \
      (defined(__HIP__) && !defined(__HIP_DEVICE_COMPILE__)))
#include <gpuintrin.h>
#define RPC_TARGET_IS_GPU
#endif

// Workaround for missing __has_builtin in < GCC 10.
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifndef RPC_ATTRS
#if defined(__CUDA__) || defined(__HIP__)
#define RPC_ATTRS __attribute__((host, device)) inline
#else
#define RPC_ATTRS inline
#endif
#endif

namespace rpc {

template <typename T> struct type_identity {
  using type = T;
};

template <typename T, T v> struct type_constant {
  static inline constexpr T value = v;
};

/// Freestanding type trait helpers.
template <typename T> struct remove_cv : type_identity<T> {};
template <typename T> struct remove_cv<const T> : type_identity<T> {};
template <typename T> using remove_cv_t = typename remove_cv<T>::type;

template <typename T> struct remove_pointer : type_identity<T> {};
template <typename T> struct remove_pointer<T *> : type_identity<T> {};
template <typename T> using remove_pointer_t = typename remove_pointer<T>::type;

template <typename T> struct remove_const : type_identity<T> {};
template <typename T> struct remove_const<const T> : type_identity<T> {};
template <typename T> using remove_const_t = typename remove_const<T>::type;

template <typename T> struct remove_reference : type_identity<T> {};
template <typename T> struct remove_reference<T &> : type_identity<T> {};
template <typename T> struct remove_reference<T &&> : type_identity<T> {};
template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

template <typename T> struct is_const : type_constant<bool, false> {};
template <typename T> struct is_const<const T> : type_constant<bool, true> {};
template <typename T> inline constexpr bool is_const_v = is_const<T>::value;

template <typename T> struct is_pointer : type_constant<bool, false> {};
template <typename T> struct is_pointer<T *> : type_constant<bool, true> {};
template <typename T>
struct is_pointer<T *const> : type_constant<bool, true> {};
template <typename T> inline constexpr bool is_pointer_v = is_pointer<T>::value;

template <typename T, typename U>
struct is_same : type_constant<bool, false> {};
template <typename T> struct is_same<T, T> : type_constant<bool, true> {};
template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

template <typename T> struct is_void : type_constant<bool, false> {};
template <> struct is_void<void> : type_constant<bool, true> {};
template <typename T> inline constexpr bool is_void_v = is_void<T>::value;

// Scary trait that can change within a TU, use with caution.
template <typename...> using void_t = void;
template <typename T, typename = void>
struct is_complete : type_constant<bool, false> {};
template <typename T>
struct is_complete<T, void_t<decltype(sizeof(T))>> : type_constant<bool, true> {
};
template <typename T>
inline constexpr bool is_complete_v = is_complete<T>::value;

template <typename T>
struct is_trivially_copyable
    : public type_constant<bool, __is_trivially_copyable(T)> {};
template <typename T>
inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<T>::value;

template <typename T, typename... Args>
struct is_trivially_constructible
    : type_constant<bool, __is_trivially_constructible(T, Args...)> {};
template <typename T, typename... Args>
inline constexpr bool is_trivially_constructible_v =
    is_trivially_constructible<T>::value;

/// Tag type to indicate an array of elements being passed through RPC.
template <typename T> struct span {
  T *data;
  uint64_t size;
  RPC_ATTRS operator T *() const { return data; }
};

template <typename T> struct is_span : type_constant<bool, false> {};
template <typename T> struct is_span<span<T>> : type_constant<bool, true> {};
template <typename T> inline constexpr bool is_span_v = is_span<T>::value;

template <typename T> struct remove_span : type_identity<T> {};
template <typename T> struct remove_span<span<T>> : type_identity<T *> {};
template <typename T> using remove_span_t = typename remove_span<T>::type;

template <bool B, typename T, typename F>
struct conditional : type_identity<T> {};
template <typename T, typename F>
struct conditional<false, T, F> : type_identity<F> {};
template <bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

/// Freestanding implementation of std::move.
template <typename T>
RPC_ATTRS constexpr typename remove_reference<T>::type &&move(T &&t) {
  return static_cast<typename remove_reference<T>::type &&>(t);
}

/// Freestanding integer sequence.
template <typename T, T... Ints> struct integer_sequence {
  template <T Next> using append = integer_sequence<T, Ints..., Next>;
};

namespace detail {
template <typename T, int N> struct make_integer_sequence {
  using type =
      typename make_integer_sequence<T, N - 1>::type::template append<N>;
};
template <typename T> struct make_integer_sequence<T, -1> {
  using type = integer_sequence<T>;
};
} // namespace detail

template <uint64_t... Ints>
using index_sequence = integer_sequence<uint64_t, Ints...>;
template <int N>
using make_index_sequence =
    typename detail::make_integer_sequence<uint64_t, N - 1>::type;
template <typename... Ts>
using index_sequence_for = make_index_sequence<sizeof...(Ts)>;

/// Freestanding implementation of std::forward.
template <typename T>
RPC_ATTRS constexpr T &&forward(typename remove_reference<T>::type &value) {
  return static_cast<T &&>(value);
}
template <typename T>
RPC_ATTRS constexpr T &&forward(typename remove_reference<T>::type &&value) {
  return static_cast<T &&>(value);
}

struct in_place_t {
  RPC_ATTRS explicit in_place_t() = default;
};

struct nullopt_t {
  RPC_ATTRS constexpr explicit nullopt_t() = default;
};

constexpr inline in_place_t in_place{};
constexpr inline nullopt_t nullopt{};

/// Freestanding and minimal implementation of std::optional.
template <typename T> struct optional {
  template <typename U> struct OptionalStorage {
    union {
      char empty;
      U stored_value;
    };

    bool in_use = false;

    RPC_ATTRS ~OptionalStorage() { reset(); }

    RPC_ATTRS constexpr OptionalStorage() : empty() {}

    template <typename... Args>
    RPC_ATTRS constexpr explicit OptionalStorage(in_place_t, Args &&...args)
        : stored_value(forward<Args>(args)...) {
      in_use = true;
    }

    RPC_ATTRS constexpr void reset() {
      if (in_use)
        stored_value.~U();
      in_use = false;
    }
  };

  OptionalStorage<T> storage;

public:
  RPC_ATTRS constexpr optional() = default;
  RPC_ATTRS constexpr optional(nullopt_t) {}

  RPC_ATTRS constexpr optional(const T &t) : storage(in_place, t) {}
  RPC_ATTRS constexpr optional(const optional &) = default;

  RPC_ATTRS constexpr optional(T &&t) : storage(in_place, move(t)) {}
  RPC_ATTRS constexpr optional(optional &&O) = default;

  template <typename... Args>
  RPC_ATTRS constexpr optional(in_place_t, Args &&...args)
      : storage(in_place, forward<Args>(args)...) {}

  RPC_ATTRS constexpr optional &operator=(T &&t) {
    storage = move(t);
    return *this;
  }
  RPC_ATTRS constexpr optional &operator=(optional &&) = default;

  RPC_ATTRS constexpr optional &operator=(const T &t) {
    storage = t;
    return *this;
  }
  RPC_ATTRS constexpr optional &operator=(const optional &) = default;

  RPC_ATTRS constexpr void reset() { storage.reset(); }

  RPC_ATTRS constexpr const T &value() const & { return storage.stored_value; }

  RPC_ATTRS constexpr T &value() & { return storage.stored_value; }

  RPC_ATTRS constexpr explicit operator bool() const { return storage.in_use; }
  RPC_ATTRS constexpr bool has_value() const { return storage.in_use; }
  RPC_ATTRS constexpr const T *operator->() const {
    return &storage.stored_value;
  }
  RPC_ATTRS constexpr T *operator->() { return &storage.stored_value; }
  RPC_ATTRS constexpr const T &operator*() const & {
    return storage.stored_value;
  }
  RPC_ATTRS constexpr T &operator*() & { return storage.stored_value; }

  RPC_ATTRS constexpr T &&value() && { return move(storage.stored_value); }
  RPC_ATTRS constexpr T &&operator*() && { return move(storage.stored_value); }
};

/// Minimal array type.
template <typename T, uint64_t N> struct array {
  T elems[N];

  RPC_ATTRS constexpr T *data() { return elems; }
  RPC_ATTRS constexpr const T *data() const { return elems; }
  RPC_ATTRS static constexpr uint64_t size() { return N; }

  RPC_ATTRS constexpr T &operator[](uint64_t i) { return elems[i]; }
  RPC_ATTRS constexpr const T &operator[](uint64_t i) const { return elems[i]; }
};

/// Minimal tuple type.
template <typename... Ts> struct tuple;
template <> struct tuple<> {};

template <typename Head, typename... Tail>
struct tuple<Head, Tail...> : tuple<Tail...> {
  Head head;

  RPC_ATTRS constexpr tuple() = default;

  template <typename OHead, typename... OTail>
  RPC_ATTRS constexpr tuple &operator=(const tuple<OHead, OTail...> &other) {
    head = other.get_head();
    this->get_tail() = other.get_tail();
    return *this;
  }

  RPC_ATTRS constexpr tuple(const Head &h, const Tail &...t)
      : tuple<Tail...>(t...), head(h) {}

  RPC_ATTRS constexpr Head &get_head() { return head; }
  RPC_ATTRS constexpr const Head &get_head() const { return head; }

  RPC_ATTRS constexpr tuple<Tail...> &get_tail() { return *this; }
  RPC_ATTRS constexpr const tuple<Tail...> &get_tail() const { return *this; }
};

template <size_t Idx, typename T> struct tuple_element;
template <size_t Idx, typename Head, typename... Tail>
struct tuple_element<Idx, tuple<Head, Tail...>>
    : tuple_element<Idx - 1, tuple<Tail...>> {};
template <typename Head, typename... Tail>
struct tuple_element<0, tuple<Head, Tail...>> {
  using type = remove_cv_t<remove_reference_t<Head>>;
};
template <size_t Idx, typename T>
using tuple_element_t = typename tuple_element<Idx, T>::type;

template <uint64_t Idx, typename Head, typename... Tail>
RPC_ATTRS constexpr auto &get(tuple<Head, Tail...> &t) {
  if constexpr (Idx == 0)
    return t.get_head();
  else
    return get<Idx - 1>(t.get_tail());
}
template <uint64_t Idx, typename Head, typename... Tail>
RPC_ATTRS constexpr const auto &get(const tuple<Head, Tail...> &t) {
  if constexpr (Idx == 0)
    return t.get_head();
  else
    return get<Idx - 1>(t.get_tail());
}

namespace detail {
template <typename F, typename Tuple, uint64_t... Is>
RPC_ATTRS auto apply(F &&f, Tuple &&t, index_sequence<Is...>) {
  return f(get<Is>(static_cast<Tuple &&>(t))...);
}
} // namespace detail

template <typename F, typename... Ts>
RPC_ATTRS auto apply(F &&f, tuple<Ts...> &t) {
  return detail::apply(static_cast<F &&>(f), t,
                       make_index_sequence<sizeof...(Ts)>{});
}

/// Suspend the thread briefly to assist the thread scheduler during busy loops.
RPC_ATTRS void sleep_briefly() {
#if __has_builtin(__nvvm_reflect)
  if (__nvvm_reflect("__CUDA_ARCH") >= 700)
    asm("nanosleep.u32 64;" ::: "memory");
#elif __has_builtin(__builtin_amdgcn_s_sleep)
  __builtin_amdgcn_s_sleep(2);
#elif __has_builtin(__builtin_ia32_pause)
  __builtin_ia32_pause();
#elif __has_builtin(__builtin_arm_isb)
  __builtin_arm_isb(0xf);
#else
  // Simply do nothing if sleeping isn't supported on this platform.
#endif
}

/// Conditional to indicate if this process is running on the GPU.
RPC_ATTRS constexpr bool is_process_gpu() {
#ifdef RPC_TARGET_IS_GPU
  return true;
#else
  return false;
#endif
}

/// Wait for all lanes in the group to complete.
RPC_ATTRS void sync_lane([[maybe_unused]] uint64_t lane_mask) {
#ifdef RPC_TARGET_IS_GPU
  return __gpu_sync_lane(lane_mask);
#endif
}

/// Copies the value from the first active thread to the rest.
RPC_ATTRS uint32_t broadcast_value([[maybe_unused]] uint64_t lane_mask,
                                   uint32_t x) {
#ifdef RPC_TARGET_IS_GPU
  return __gpu_read_first_lane_u32(lane_mask, x);
#else
  return x;
#endif
}

/// Returns the number lanes that participate in the RPC interface.
RPC_ATTRS uint32_t get_num_lanes() {
#ifdef RPC_TARGET_IS_GPU
  return __gpu_num_lanes();
#else
  return 1;
#endif
}

/// Returns a bitmask of the currently active lanes.
RPC_ATTRS uint64_t get_lane_mask() {
#ifdef RPC_TARGET_IS_GPU
  return __gpu_lane_mask();
#else
  return 1;
#endif
}

/// Returns the id of the thread inside of an AMD wavefront executing together.
RPC_ATTRS uint32_t get_lane_id() {
#ifdef RPC_TARGET_IS_GPU
  return __gpu_lane_id();
#else
  return 0;
#endif
}

/// Conditional that is only true for a single thread in a lane.
RPC_ATTRS bool is_first_lane([[maybe_unused]] uint64_t lane_mask) {
#ifdef RPC_TARGET_IS_GPU
  return __gpu_is_first_in_lane(lane_mask);
#else
  return true;
#endif
}

/// Returns a bitmask of threads in the current lane for which \p x is true.
RPC_ATTRS uint64_t ballot([[maybe_unused]] uint64_t lane_mask, bool x) {
#ifdef RPC_TARGET_IS_GPU
  return __gpu_ballot(lane_mask, x);
#else
  return x;
#endif
}

/// Signal an interrupt from the device to wake the server. Only supported for
/// AMDGPU targets currently.
RPC_ATTRS void signal_interrupt([[maybe_unused]] uint32_t event_id) {
#ifdef __AMDGPU__
  constexpr uint32_t MSG_INTERRUPT = 1;
  __builtin_amdgcn_s_sendmsg(MSG_INTERRUPT, event_id);
#endif
}

/// Return \p val aligned "upwards" according to \p align.
template <typename V, typename A>
RPC_ATTRS constexpr V align_up(V val, A align) {
  return ((val + V(align) - 1) / V(align)) * V(align);
}

/// Utility to provide a unified interface between the CPU and GPU's memory
/// model. On the GPU stack variables are always private to a lane so we can
/// simply use the variable passed in. On the CPU we need to allocate enough
/// space for the whole lane and index into it.
template <typename V> RPC_ATTRS V &lane_value(V *val, uint32_t id) {
  if constexpr (is_process_gpu())
    return *val;
  return val[id];
}

/// Advance the \p p by \p bytes.
template <typename T, typename U> RPC_ATTRS T *advance(T *ptr, U bytes) {
  if constexpr (is_const<T>::value)
    return reinterpret_cast<T *>(reinterpret_cast<const uint8_t *>(ptr) +
                                 bytes);
  else
    return reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(ptr) + bytes);
}

/// Wrapper around the optimal memory copy implementation for the target.
RPC_ATTRS void rpc_memcpy(void *dst, const void *src, uint64_t count) {
#if __has_builtin(__builtin_memcpy)
  if (count)
    __builtin_memcpy(dst, src, count);
#else
  for (uint64_t i = 0; i < count; ++i)
    static_cast<uint8_t *>(dst)[i] = static_cast<const uint8_t *>(src)[i];
#endif
}

/// Minimal string length function.
RPC_ATTRS constexpr uint64_t string_length(const char *s) {
  const char *end = s;
  for (; *end != '\0'; ++end)
    ;
  return static_cast<uint64_t>(end - s + 1);
}

/// Helper for dealing with function pointers and lambda types.
template <typename> struct function_traits;
template <typename R, typename... Args> struct function_traits<R (*)(Args...)> {
  using return_type = R;
  using arg_types = rpc::tuple<Args...>;
  static constexpr uint64_t ARITY = sizeof...(Args);
};
template <typename R, typename... Args>
struct function_traits<R (*)(Args...) noexcept> {
  using return_type = R;
  using arg_types = rpc::tuple<Args...>;
  static constexpr uint64_t ARITY = sizeof...(Args);
};
template <typename T> T &&declval();
template <typename T>
struct function_traits
    : function_traits<decltype(+declval<rpc::remove_reference_t<T>>())> {};

template <typename T, typename U>
RPC_ATTRS constexpr T max(const T &a, const U &b) {
  return (a < b) ? b : a;
}

template <typename T, typename U>
RPC_ATTRS constexpr T min(const T &a, const U &b) {
  return (a < b) ? a : b;
}

} // namespace rpc

#endif // LLVM_LIBC_SHARED_RPC_UTIL_H
