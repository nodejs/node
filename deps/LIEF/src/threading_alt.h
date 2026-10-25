/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#pragma once

// NOLINTBEGIN
typedef struct mbedtls_platform_mutex_t {
  void* mutex;
} mbedtls_platform_mutex_t;

typedef struct mbedtls_platform_condition_variable_t {
  void* cv;
} mbedtls_platform_condition_variable_t;

void mbedtls_init_threading_alt();

// NOLINTEND
