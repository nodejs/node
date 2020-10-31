/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACING_IPC_MEMFD_H_
#define SRC_TRACING_IPC_MEMFD_H_

#include "perfetto/base/build_config.h"

#include "perfetto/ext/base/scoped_file.h"

// Some android build bots use a sysroot that doesn't support memfd when
// compiling for the host, so we define the flags we need ourselves.

// from memfd.h
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#define MFD_ALLOW_SEALING 0x0002U
#endif

// from fcntl.h
#ifndef F_ADD_SEALS
#define F_ADD_SEALS 1033
#define F_GET_SEALS 1034
#define F_SEAL_SEAL 0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW 0x0004
#define F_SEAL_WRITE 0x0008
#endif

namespace perfetto {

// Whether the operating system supports memfd.
bool HasMemfdSupport();

// Call memfd(2) if available on platform and return the fd as result. This call
// also makes a kernel version check for safety on older kernels (b/116769556).
// Returns an invalid ScopedFile on failure.
base::ScopedFile CreateMemfd(const char* name, unsigned int flags);

}  // namespace perfetto

#endif  // SRC_TRACING_IPC_MEMFD_H_
