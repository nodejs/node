//===-- Shared memory RPC client instantiation ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "rpc_client.h"

#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace rpc {

/// The libc client instance used to communicate with the server. Externally
/// visible symbol to signify the usage of an RPC client to whomever needs to
/// run the server as well as provide a way to initialize the client.
[[gnu::visibility("protected")]] Client client;

} // namespace rpc
} // namespace LIBC_NAMESPACE_DECL
