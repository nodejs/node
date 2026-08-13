//===-------------- GPU implementation of IO utils --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "io.h"

#include "src/__support/CPP/string_view.h"
#include "src/__support/RPC/rpc_client.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

void write_to_stderr(cpp::string_view msg) {
  rpc::Client::Port port = rpc::client.open<LIBC_WRITE_TO_STDERR>();
  port.send_n(msg.data(), msg.size());
  port.recv([](rpc::Buffer *, uint32_t) { /* void */ });
}

} // namespace LIBC_NAMESPACE_DECL
