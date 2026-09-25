/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
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

#ifndef LIEF_SYMBOL_VISIBILITY_H
#define LIEF_SYMBOL_VISIBILITY_H

/* Thanks to https://github.com/aguinet/dragonffi/blob/40f3fecb9530a2ef840f63882c5284ea5e8dc9e8/include/dffi/exports.h */
#if defined _WIN32 || defined __CYGWIN__
  #define LIEF_HELPER_IMPORT __declspec(dllimport)
  #define LIEF_HELPER_EXPORT __declspec(dllexport)
  #define LIEF_HELPER_LOCAL
#else
  #define LIEF_HELPER_IMPORT __attribute__ ((visibility ("default")))
  #define LIEF_HELPER_EXPORT __attribute__ ((visibility ("default")))
  #define LIEF_HELPER_LOCAL  __attribute__ ((visibility ("hidden")))
#endif

#if defined(LIEF_STATIC)
  #define LIEF_API
  #define LIEF_LOCAL
#elif defined(LIEF_EXPORTS)
  #define LIEF_API   LIEF_HELPER_EXPORT
  #define LIEF_LOCAL LIEF_HELPER_LOCAL
#elif defined(LIEF_IMPORT)
  #define LIEF_API   LIEF_HELPER_IMPORT
  #define LIEF_LOCAL LIEF_HELPER_LOCAL
#else
  #define LIEF_API
  #define LIEF_LOCAL LIEF_HELPER_LOCAL
#endif

#endif
