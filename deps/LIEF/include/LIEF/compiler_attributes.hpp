/* Copyright 2021 - 2026 R. Thomas
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
  #if __cplusplus >= 201103L
    #define LIEF_CPP11
    #if __cplusplus >= 201402L
      #define LIEF_CPP14
      #if __cplusplus >= 201703L
        #define LIEF_CPP17
        #if __cplusplus >= 202002L
          #define LIEF_CPP20
        #endif
      #endif
    #endif
  #endif
#elif defined(_MSC_VER)
  #if _MSVC_LANG >= 201103L
    #define LIEF_CPP11
    #if _MSVC_LANG >= 201402L
      #define LIEF_CPP14
      #if _MSVC_LANG > 201402L
        #define LIEF_CPP17
        #if _MSVC_LANG >= 202002L
          #define LIEF_CPP20
        #endif
      #endif
    #endif
  #endif
#endif

#if defined(__MINGW32__)
  #define LIEF_DEPRECATED(reason)
#elif defined(LIEF_CPP14)
  #define LIEF_DEPRECATED(reason) [[deprecated(reason)]]
#else
  #define LIEF_DEPRECATED(reason) __attribute__((deprecated(reason)))
#endif

#if defined(LIEF_CPP17)
  #define LIEF_MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
  #define LIEF_MAYBE_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
  #define LIEF_MAYBE_UNUSED __pragma(warning(suppress : 4100 4101))
#else
  #define LIEF_MAYBE_UNUSED
#endif

#if defined(__has_cpp_attribute)
  #if __has_cpp_attribute(clang::lifetimebound)
    #define LIEF_LIFETIMEBOUND [[clang::lifetimebound]]
  #else
    #define LIEF_LIFETIMEBOUND
  #endif
#else
  #define LIEF_LIFETIMEBOUND
#endif

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
  #include <ranges>
  #define LIEF_HAS_STD_RANGES 1
#else
  #define LIEF_HAS_STD_RANGES 0
#endif


#endif
