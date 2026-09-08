//===-- Shared memory RPC client instantiation ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_RPC_RPC_CLIENT_H
#define LLVM_LIBC_SRC___SUPPORT_RPC_RPC_CLIENT_H

#include "shared/rpc.h"
#include "shared/rpc_opcodes.h"

#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace rpc {

using ::rpc::Buffer;
using ::rpc::Client;
using ::rpc::Port;
using ::rpc::Process;
using ::rpc::Server;

static_assert(cpp::is_trivially_copyable<Client>::value &&
                  sizeof(Process<true>) == sizeof(Process<false>),
              "The client is not trivially copyable from the server");

/// The libc client instance used to communicate with the server.
[[gnu::visibility("protected")]] extern Client client asm("__llvm_rpc_client");

} // namespace rpc
} // namespace LIBC_NAMESPACE_DECL

#endif
