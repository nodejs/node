//===-- Helper functions for client / server dispatch -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_RPC_DISPATCH_H
#define LLVM_LIBC_SHARED_RPC_DISPATCH_H

#include "rpc.h"
#include "rpc_util.h"

namespace rpc {
namespace {

// Forward declarations needed for the server, we assume these are present.
extern "C" void *malloc(__SIZE_TYPE__);
extern "C" void free(void *);

// Traits to convert between a tuple and binary representation of an argument
// list.
template <typename... Ts> struct tuple_bytes {
  static constexpr uint64_t SIZE = rpc::max(1ul, (0 + ... + sizeof(Ts)));
  using array_type = rpc::array<uint8_t, SIZE>;

  template <uint64_t... Is>
  RPC_ATTRS static constexpr array_type pack_impl(rpc::tuple<Ts...> t,
                                                  rpc::index_sequence<Is...>) {
    array_type out{};
    uint8_t *p = out.data();
    ((rpc::rpc_memcpy(p, &rpc::get<Is>(t), sizeof(Ts)), p += sizeof(Ts)), ...);
    return out;
  }

  RPC_ATTRS static constexpr array_type pack(rpc::tuple<Ts...> t) {
    return pack_impl(t, rpc::index_sequence_for<Ts...>{});
  }

  template <uint64_t... Is>
  RPC_ATTRS static constexpr rpc::tuple<Ts...>
  unpack_impl(const uint8_t *data, rpc::index_sequence<Is...>) {
    rpc::tuple<Ts...> t{};
    const uint8_t *p = data;
    ((rpc::rpc_memcpy(&rpc::get<Is>(t), p, sizeof(Ts)), p += sizeof(Ts)), ...);
    return t;
  }

  RPC_ATTRS static constexpr rpc::tuple<Ts...> unpack(const array_type &a) {
    return unpack_impl(a.data(), rpc::index_sequence_for<Ts...>{});
  }
};
template <typename... Ts>
struct tuple_bytes<rpc::tuple<Ts...>> : tuple_bytes<Ts...> {};

// Whether a pointer value will be marshalled between the client and server.
template <typename Ty, typename PtrTy = rpc::remove_reference_t<Ty>,
          typename ElemTy = rpc::remove_pointer_t<PtrTy>>
RPC_ATTRS constexpr bool is_marshalled_ptr_v =
    rpc::is_pointer_v<PtrTy> && rpc::is_complete_v<ElemTy> &&
    !rpc::is_void_v<ElemTy>;

// Get an index value for the marshalled types in the tuple type.
template <class Tuple, uint64_t... Is>
constexpr uint64_t marshalled_index(rpc::index_sequence<Is...>) {
  return (0u + ... +
          (rpc::is_marshalled_ptr_v<rpc::tuple_element_t<Is, Tuple>>));
}
template <class Tuple, uint64_t I>
constexpr uint64_t marshalled_index_v =
    marshalled_index<Tuple>(rpc::make_index_sequence<I>{});

// Storage for the marshalled arguments from the client.
template <uint32_t NUM_LANES, typename... Ts> struct MarshalledState {
  static constexpr uint32_t NUM_PTRS =
      rpc::marshalled_index_v<rpc::tuple<Ts...>, sizeof...(Ts)>;

  uint64_t sizes[NUM_PTRS][NUM_LANES]{};
  void *ptrs[NUM_PTRS][NUM_LANES]{};
};
template <uint32_t NUM_LANES, typename... Ts>
struct MarshalledState<NUM_LANES, rpc::tuple<Ts...>>
    : MarshalledState<NUM_LANES, Ts...> {};

// Client-side dispatch of pointer values. We copy the memory associated with
// the pointer to the server and receive back a server-side pointer to replace
// the client-side pointer in the argument list.
template <uint64_t Idx, typename Tuple, typename CallTuple>
RPC_ATTRS constexpr void prepare_arg(rpc::Client::Port &port, Tuple &t,
                                     CallTuple &ct) {
  using ArgTy = rpc::tuple_element_t<Idx, Tuple>;
  using CallArgTy = rpc::tuple_element_t<Idx, CallTuple>;
  if constexpr (rpc::is_marshalled_ptr_v<ArgTy>) {
    // We assume all constant character arrays are C-strings.
    uint64_t size{};
    if (rpc::get<Idx>(t)) {
      if constexpr (rpc::is_span_v<CallArgTy>)
        size = rpc::get<Idx>(ct).size * sizeof(rpc::remove_pointer_t<ArgTy>);
      else if constexpr (rpc::is_same_v<ArgTy, const char *>)
        size = rpc::string_length(rpc::get<Idx>(t));
      else
        size = sizeof(rpc::remove_pointer_t<ArgTy>);
    }
    port.send_n(rpc::get<Idx>(t), size);
    port.recv([&](rpc::Buffer *buffer, uint32_t) {
      ArgTy val;
      rpc::rpc_memcpy(&val, buffer->data, sizeof(ArgTy));
      rpc::get<Idx>(t) = val;
    });
  }
}

// Server-side handling of pointer arguments. We receive the memory into a
// temporary buffer and pass a pointer to this new memory back to the client.
template <uint32_t NUM_LANES, typename Tuple, uint64_t Idx, typename State>
RPC_ATTRS constexpr void prepare_arg(rpc::Server::Port &port, State &&state) {
  using ArgTy = rpc::tuple_element_t<Idx, Tuple>;
  if constexpr (rpc::is_marshalled_ptr_v<ArgTy>) {
    auto &ptrs = state.ptrs[rpc::marshalled_index_v<Tuple, Idx>];
    auto &sizes = state.sizes[rpc::marshalled_index_v<Tuple, Idx>];
    port.recv_n(ptrs, sizes,
                [](uint64_t size) { return size ? malloc(size) : 0; });
    port.send([&](rpc::Buffer *buffer, uint32_t id) {
      ArgTy val = static_cast<ArgTy>(ptrs[id]);
      rpc::rpc_memcpy(buffer->data, &val, sizeof(ArgTy));
    });
  }
}

// Client-side finalization of pointer arguments. If the type is not constant we
// must copy back any potential modifications the invoked function made to that
// pointer.
template <uint64_t Idx, typename Tuple>
RPC_ATTRS constexpr void finish_arg(rpc::Client::Port &port, Tuple &t) {
  using ArgTy = rpc::tuple_element_t<Idx, Tuple>;
  if constexpr (rpc::is_marshalled_ptr_v<ArgTy> && !rpc::is_const_v<ArgTy>) {
    uint64_t size{};
    void *buf{};
    port.recv_n(&buf, &size, [&](uint64_t) {
      return const_cast<void *>(static_cast<const void *>(rpc::get<Idx>(t)));
    });
  }
}

// Server-side finalization of pointer arguments. We copy any potential
// modifications to the value back to the client unless it was a constant. We
// can also free the associated memory.
template <uint32_t NUM_LANES, typename Tuple, uint64_t Idx, typename State>
RPC_ATTRS constexpr void finish_arg(rpc::Server::Port &port, State &&state) {
  using ArgTy = rpc::tuple_element_t<Idx, Tuple>;
  if constexpr (rpc::is_marshalled_ptr_v<ArgTy> && !rpc::is_const_v<ArgTy>) {
    auto &ptrs = state.ptrs[rpc::marshalled_index_v<Tuple, Idx>];
    auto &sizes = state.sizes[rpc::marshalled_index_v<Tuple, Idx>];
    port.send_n(ptrs, sizes);
  }

  if constexpr (rpc::is_marshalled_ptr_v<ArgTy>) {
    auto &ptrs = state.ptrs[rpc::marshalled_index_v<Tuple, Idx>];
    for (uint32_t id = 0; id < NUM_LANES; ++id) {
      if (port.get_lane_mask() & (uint64_t(1) << id))
        free(const_cast<void *>(static_cast<const void *>(ptrs[id])));
    }
  }
}

// Iterate over the tuple list of arguments to see if we need to forward any.
// The current forwarding is somewhat inefficient as each pointer is an
// individual RPC call.
template <typename Tuple, typename CallTuple, uint64_t... Is>
RPC_ATTRS constexpr void prepare_args(rpc::Client::Port &port, Tuple &t,
                                      CallTuple &ct,
                                      rpc::index_sequence<Is...>) {
  (prepare_arg<Is>(port, t, ct), ...);
}
template <uint32_t NUM_LANES, typename Tuple, typename State, uint64_t... Is>
RPC_ATTRS constexpr void prepare_args(rpc::Server::Port &port, State &&state,
                                      rpc::index_sequence<Is...>) {
  (prepare_arg<NUM_LANES, Tuple, Is>(port, state), ...);
}

// Performs the preparation in reverse, copying back any modified values.
template <typename Tuple, uint64_t... Is>
RPC_ATTRS constexpr void finish_args(rpc::Client::Port &port, Tuple &&t,
                                     rpc::index_sequence<Is...>) {
  (finish_arg<Is>(port, t), ...);
}
template <uint32_t NUM_LANES, typename Tuple, typename State, uint64_t... Is>
RPC_ATTRS constexpr void finish_args(rpc::Server::Port &port, State &&state,
                                     rpc::index_sequence<Is...>) {
  (finish_arg<NUM_LANES, Tuple, Is>(port, state), ...);
}
} // namespace

// Dispatch a function call to the server through the RPC mechanism. Copies the
// argument list through the RPC interface.
template <uint32_t OPCODE, auto Fn, typename... CallArgs>
RPC_ATTRS constexpr typename function_traits<decltype(Fn)>::return_type
dispatch(rpc::Client &client, CallArgs... args) {
  using Traits = function_traits<decltype(Fn)>;
  using RetTy = typename Traits::return_type;
  using TupleTy = typename Traits::arg_types;
  using Bytes = tuple_bytes<rpc::remove_span_t<CallArgs>...>;

  static_assert(sizeof...(CallArgs) == Traits::ARITY,
                "Argument count mismatch");
  static_assert(((rpc::is_trivially_constructible_v<CallArgs> &&
                  rpc::is_trivially_copyable_v<CallArgs>) &&
                 ...),
                "Must be a trivial type");

  auto port = client.open<OPCODE>();

  // Copy over any pointer arguments by walking the argument list.
  rpc::tuple<CallArgs...> call_args{args...};
  TupleTy arg_tuple{rpc::forward<CallArgs>(args)...};
  rpc::prepare_args(port, arg_tuple, call_args,
                    rpc::make_index_sequence<Traits::ARITY>{});

  // Compress the argument list to a binary stream and send it to the server.
  auto bytes = Bytes::pack(arg_tuple);
  port.send_n(&bytes);

  // Copy back any potentially modified pointer arguments and the return value.
  rpc::finish_args(port, TupleTy{rpc::forward<CallArgs>(args)...},
                   rpc::make_index_sequence<Traits::ARITY>{});

  // Copy back the final function return value.
  using BufferTy = rpc::conditional_t<rpc::is_void_v<RetTy>, uint8_t, RetTy>;
  BufferTy ret{};
  port.recv_n(&ret);

  if constexpr (!rpc::is_void_v<RetTy>)
    return ret;
}

// Invoke a function on the server on behalf of the client. Receives the
// arguments through the interface and forwards them to the function.
template <uint32_t NUM_LANES, typename FnTy>
RPC_ATTRS constexpr void invoke(rpc::Server::Port &port, FnTy fn) {
  using Traits = function_traits<FnTy>;
  using RetTy = typename Traits::return_type;
  using TupleTy = typename Traits::arg_types;
  using Bytes = tuple_bytes<TupleTy>;

  // Receive pointer data from the host and pack it in server-side memory.
  MarshalledState<NUM_LANES, TupleTy> state{};
  rpc::prepare_args<NUM_LANES, TupleTy>(
      port, state, rpc::make_index_sequence<Traits::ARITY>{});

  // Get the argument list from the client.
  typename Bytes::array_type arg_buf[NUM_LANES]{};
  port.recv_n(arg_buf);

  // Convert the received arguments into an invocable argument list.
  TupleTy args[NUM_LANES];
  for (uint32_t id = 0; id < NUM_LANES; ++id) {
    if (port.get_lane_mask() & (uint64_t(1) << id))
      args[id] = Bytes::unpack(arg_buf[id]);
  }

  // Execute the function with the provided arguments and send back any copies
  // made for pointer data.
  using BufferTy = rpc::conditional_t<rpc::is_void_v<RetTy>, uint8_t, RetTy>;
  BufferTy rets[NUM_LANES]{};
  for (uint32_t id = 0; id < NUM_LANES; ++id) {
    if (port.get_lane_mask() & (uint64_t(1) << id)) {
      if constexpr (rpc::is_void_v<RetTy>)
        rpc::apply(fn, args[id]);
      else
        rets[id] = rpc::apply(fn, args[id]);
    }
  }

  // Send any potentially modified pointer arguments back to the client.
  rpc::finish_args<NUM_LANES, TupleTy>(
      port, state, rpc::make_index_sequence<Traits::ARITY>{});

  // Copy back the return value of the function if one exists. If the function
  // is void we send an empty packet to force synchronous behavior.
  port.send_n(rets);
}
} // namespace rpc

#endif // LLVM_LIBC_SHARED_RPC_DISPATCH_H
