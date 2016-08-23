// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Collections_h
#define Collections_h

#include <cstddef>

#if defined(__APPLE__) && !defined(_LIBCPP_VERSION)
#include <map>
#include <set>

namespace blink {
namespace protocol {

template <class Key, class T> using HashMap = std::map<Key, T>;
template <class Key> using HashSet = std::set<Key>;

} // namespace protocol
} // namespace blink

#else
#include <unordered_map>
#include <unordered_set>

namespace blink {
namespace protocol {

template <class Key, class T> using HashMap = std::unordered_map<Key, T>;
template <class Key> using HashSet = std::unordered_set<Key>;

} // namespace protocol
} // namespace blink

#endif // defined(__APPLE__) && !defined(_LIBCPP_VERSION)

// Macro that returns a compile time constant with the length of an array, but gives an error if passed a non-array.
template<typename T, std::size_t Size> char (&ArrayLengthHelperFunction(T (&)[Size]))[Size];
// GCC needs some help to deduce a 0 length array.
#if defined(__GNUC__)
template<typename T> char (&ArrayLengthHelperFunction(T (&)[0]))[0];
#endif
#define PROTOCOL_ARRAY_LENGTH(array) sizeof(::ArrayLengthHelperFunction(array))

#endif // !defined(Collections_h)
