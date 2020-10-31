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

#ifndef BUILDTOOLS_LIBBACKTRACE_CONFIG_CONFIG_H_
#define BUILDTOOLS_LIBBACKTRACE_CONFIG_CONFIG_H_

#if defined(__i386__) || defined(__ARMEL__)
#define BACKTRACE_ELF_SIZE 32
#else
#define BACKTRACE_ELF_SIZE 64
#endif

#define BACKTRACE_XCOFF_SIZE unused
#define HAVE_ATOMIC_FUNCTIONS 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_DECL_STRNLEN 1
#define HAVE_DLFCN_H 1
#define HAVE_FCNTL 1
#define HAVE_INTTYPES_H 1
#define HAVE_LSTAT 1
#define HAVE_MEMORY_H 1
#define HAVE_READLINK 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYNC_FUNCTIONS 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_LINK_H 1
#define STDC_HEADERS 1
#define HAVE_DL_ITERATE_PHDR 1

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#endif  // BUILDTOOLS_LIBBACKTRACE_CONFIG_CONFIG_H_
