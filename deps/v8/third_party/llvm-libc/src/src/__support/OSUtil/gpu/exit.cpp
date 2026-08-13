//===------------- GPU implementation of an exit function -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/OSUtil/exit.h"

#include "src/__support/GPU/utils.h"
#include "src/__support/RPC/rpc_client.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

[[noreturn]] void exit(int status) {
  // We want to first make sure the server is listening before we exit.
  rpc::Client::Port port = rpc::client.open<LIBC_EXIT>();
  port.send_and_recv([](rpc::Buffer *, uint32_t) {},
                     [](rpc::Buffer *, uint32_t) {});
  port.send([&](rpc::Buffer *buffer, uint32_t) {
    reinterpret_cast<uint32_t *>(buffer->data)[0] = status;
  });

  gpu::end_program();
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL
