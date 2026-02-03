/* Copyright 2021 - 2025 R. Thomas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_COMPILER_ATTR_H
#define LIEF_COMPILER_ATTR_H

#if !defined(_MSC_VER)
#   if __cplusplus >= 201103L
#     define LIEF_CPP11
#     if __cplusplus >= 201402L
#       define LIEF_CPP14
#       if __cplusplus >= 201703L
#         define LIEF_CPP17
#         if __cplusplus >= 202002L
#           define LIEF_CPP20
#         endif
#       endif
#     endif
#   endif
#elif defined(_MSC_VER)
#   if _MSVC_LANG >= 201103L
#     define LIEF_CPP11
#     if _MSVC_LANG >= 201402L
#       define LIEF_CPP14
#       if _MSVC_LANG > 201402L
#         define LIEF_CPP17
#         if _MSVC_LANG >= 202002L
#           define LIEF_CPP20
#         endif
#       endif
#     endif
#   endif
#endif

#if defined(__MINGW32__)
#   define LIEF_DEPRECATED(reason)
#elif defined(LIEF_CPP14)
#   define LIEF_DEPRECATED(reason) [[deprecated(reason)]]
#else
#   define LIEF_DEPRECATED(reason) __attribute__((deprecated(reason)))
#endif

#endif
