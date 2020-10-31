/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_BASE_EXPORT_H_
#define INCLUDE_PERFETTO_BASE_EXPORT_H_

#include "perfetto/base/build_config.h"

#if PERFETTO_BUILDFLAG(PERFETTO_COMPONENT_BUILD)

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#if defined(PERFETTO_IMPLEMENTATION)
#define PERFETTO_EXPORT __declspec(dllexport)
#else
#define PERFETTO_EXPORT __declspec(dllimport)
#endif

#else  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#if defined(PERFETTO_IMPLEMENTATION)
#define PERFETTO_EXPORT __attribute__((visibility("default")))
#else
#define PERFETTO_EXPORT
#endif

#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#else  // !PERFETTO_BUILDFLAG(PERFETTO_COMPONENT_BUILD)

#define PERFETTO_EXPORT

#endif  // PERFETTO_BUILDFLAG(PERFETTO_COMPONENT_BUILD)

#endif  // INCLUDE_PERFETTO_BASE_EXPORT_H_
