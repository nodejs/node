/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SRC_IPC_TEST_TEST_SOCKET_H_
#define SRC_IPC_TEST_TEST_SOCKET_H_

#include "perfetto/base/build_config.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#define TEST_SOCK_NAME(x) "@" x
#define DESTROY_TEST_SOCK(x) \
  do {                       \
  } while (0)
#else
#include <unistd.h>
#define TEST_SOCK_NAME(x) "/tmp/" x ".sock"
#define DESTROY_TEST_SOCK(x) unlink(x)
#endif

#endif  // SRC_IPC_TEST_TEST_SOCKET_H_
