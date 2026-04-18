/*
 * Frozen
 * Copyright 2016 QuarksLab
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef FROZEN_LETITGO_DEFINES_H
#define FROZEN_LETITGO_DEFINES_H

#if defined(_MSVC_LANG) && !(defined(__EDG__) && defined(__clang__)) // TRANSITION, VSO#273681
  #define FROZEN_LETITGO_IS_MSVC
#endif

// Code taken from https://stackoverflow.com/questions/43639122/which-values-can-msvc-lang-have
#if defined(FROZEN_LETITGO_IS_MSVC)
  #if _MSVC_LANG > 201402
    #define FROZEN_LETITGO_HAS_CXX17  1
  #else /* _MSVC_LANG > 201402 */
    #define FROZEN_LETITGO_HAS_CXX17  0
  #endif /* _MSVC_LANG > 201402 */
#else /* _MSVC_LANG etc. */
  #if __cplusplus > 201402
    #define FROZEN_LETITGO_HAS_CXX17  1
  #else /* __cplusplus > 201402 */
    #define FROZEN_LETITGO_HAS_CXX17  0
  #endif /* __cplusplus > 201402 */
#endif /* _MSVC_LANG etc. */
// End if taken code

#if FROZEN_LETITGO_HAS_CXX17 == 1 && defined(FROZEN_LETITGO_IS_MSVC)
  #define FROZEN_LETITGO_HAS_STRING_VIEW // We assume Visual Studio always has string_view in C++17
#else
  #if FROZEN_LETITGO_HAS_CXX17 == 1 && __has_include(<string_view>)
    #define FROZEN_LETITGO_HAS_STRING_VIEW
  #endif
#endif

#ifdef __cpp_char8_t
  #define FROZEN_LETITGO_HAS_CHAR8T
#endif

#if __cpp_deduction_guides >= 201703L
  #define FROZEN_LETITGO_HAS_DEDUCTION_GUIDES
#endif

#if __cpp_lib_constexpr_string >= 201907L
  #define FROZEN_LETITGO_HAS_CONSTEXPR_STRING
#endif

#endif // FROZEN_LETITGO_DEFINES_H
