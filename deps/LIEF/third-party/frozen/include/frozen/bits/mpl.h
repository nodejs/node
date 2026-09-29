/*
 * Frozen
 * Copyright 2022 Giel van Schijndel
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

#ifndef FROZEN_LETITGO_BITS_MPL_H
#define FROZEN_LETITGO_BITS_MPL_H

#include <utility>

namespace frozen {

namespace bits {

// Forward declarations
template <class, std::size_t>
class carray;

template <typename T>
struct remove_cv : std::remove_cv<T> {};

template <typename... T>
struct remove_cv<std::pair<T...>> {
  using type = std::pair<typename remove_cv<T>::type...>;
};

template <typename T, std::size_t N>
struct remove_cv<carray<T, N>> {
  using type = carray<typename remove_cv<T>::type, N>;
};

template <typename T>
using remove_cv_t = typename remove_cv<T>::type;

} // namespace bits

} // namespace frozen

#endif
