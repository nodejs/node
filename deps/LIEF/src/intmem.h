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
/*
 * Thanks to Adrien Guinet (@adriengnt) for these helpers:
 * https://github.com/aguinet/intmem
 */
#ifndef INTMEM_H
#define INTMEM_H

#if __cplusplus >= 202002L
#define INTMEM_CE constexpr
#define INTMEM_CPP20_SUPPORT
#else
#define INTMEM_CE
#endif

#include <cstdint>
#include <cstring>
#include <type_traits>
#ifdef INTMEM_CPP20_SUPPORT
#include <bit>
#endif

namespace intmem {

#ifdef INTMEM_CPP20_SUPPORT
template <class T>
static INTMEM_CE T bswap_ce(T V)
{
  static_assert(std::is_unsigned_v<T>);
  T Ret = 0;
  for (size_t I = 0; I < sizeof(T); ++I) {
    Ret |= ((V >> ((sizeof(T)-I-1)*8)) & 0xFF) << (I*8);
  }
  return Ret;
}
#endif

// bswap functions. Uses GCC/clang/MSVC intrinsics.
#ifdef _MSC_VER
#include <stdlib.h>
static uint8_t  bswap_rt(uint8_t V) { return V; }
static uint16_t bswap_rt(unsigned short V) { return _byteswap_ushort(V); }
static_assert(sizeof(uint32_t) == sizeof(unsigned long), "unsigned long isn't 32-bit wide!");
static uint32_t bswap_rt(uint32_t V) { return _byteswap_ulong(V); }
static uint64_t bswap_rt(uint64_t V) { return _byteswap_uint64(V); }
#else
static uint8_t  bswap_rt(uint8_t V) { return V; }
static uint16_t bswap_rt(uint16_t V) { return __builtin_bswap16(V); }
static uint32_t bswap_rt(uint32_t V) { return __builtin_bswap32(V); }
static uint64_t bswap_rt(uint64_t V) { return __builtin_bswap64(V); }
#endif

template <class T>
static INTMEM_CE T bswap(const T V) {
#ifdef INTMEM_CPP20_SUPPORT
  if (std::is_constant_evaluated()) {
    return bswap_ce(V);
  }
#endif
  return bswap_rt(V);
}

static INTMEM_CE int8_t  bswap(int8_t V) { return V; }
static INTMEM_CE int16_t bswap(int16_t V) {
#ifdef INTMEM_CPP20_SUPPORT
  return std::bit_cast<int16_t>(bswap(std::bit_cast<uint16_t>(V)));
#else
  return bswap((uint16_t)V);
#endif
}
static INTMEM_CE int32_t bswap(int32_t V) {
#ifdef INTMEM_CPP20_SUPPORT
  return std::bit_cast<int32_t>(bswap(std::bit_cast<uint32_t>(V)));
#else
  return bswap((uint32_t)V);
#endif
}
static INTMEM_CE int64_t bswap(int64_t V) {
#ifdef INTMEM_CPP20_SUPPORT
  return std::bit_cast<int64_t>(bswap(std::bit_cast<uint64_t>(V)));
#else
  return bswap((uint64_t)V);
#endif
}

template <class T>
static T loadu(const void* Ptr)
{
  static_assert(std::is_integral<T>::value, "T must be an integer!");
  T Ret;
  memcpy(&Ret, Ptr, sizeof(T));
  return Ret;
}

template <class T>
static void storeu(void* Ptr, T const V)
{
  static_assert(std::is_integral<T>::value, "T must be an integer!");
  memcpy(Ptr, &V, sizeof(V));
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
template <class T>
static T bswap_le(T const v)
{
  return v;
}

template <class T>
static T bswap_be(T const v)
{
  return bswap(v);
}
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
template <class T>
static T bswap_le(T const v)
{
  return bswap(v);
}

template <class T>
static T bswap_be(T const v)
{
  return v;
}
#else
#error Unsupported endianess!
#endif

template <class T>
static T loadu_le(const void* Ptr)
{
  return loadu_le<T>(reinterpret_cast<const uint8_t*>(Ptr));
}

template <class T>
static INTMEM_CE T loadu_le(const uint8_t* Ptr)
{
#ifdef INTMEM_CPP20_SUPPORT
  if (std::is_constant_evaluated()) {
    T Ret = 0;
    for (size_t I = 0; I < sizeof(T); ++I) {
      Ret |= ((T)(Ptr[I])) << (I*8);
    }
    return Ret;
  }
#endif
  return bswap_le(loadu<T>(Ptr));
}

template <class T>
static T loadu_be(const void* Ptr)
{
  return loadu_be<T>(reinterpret_cast<const uint8_t*>(Ptr));
}

template <class T>
static INTMEM_CE T loadu_be(const uint8_t* Ptr)
{
#ifdef INTMEM_CPP20_SUPPORT
  if (std::is_constant_evaluated()) {
    T Ret = 0;
    for (size_t I = 0; I < sizeof(T); ++I) {
      Ret |= ((T)Ptr[I]) << ((sizeof(T)-I-1)*8);
    }
    return Ret;
  }
#endif
  return bswap_be(loadu<T>(Ptr));
}

template <class T>
static INTMEM_CE void storeu_le(void* Ptr, T const V)
{
  storeu(Ptr, bswap_le(V));
}

template <class T>
static INTMEM_CE void storeu_be(void* Ptr, T const V)
{
  storeu(Ptr, bswap_be(V));
}

template <class T>
static INTMEM_CE T load_le(const T* Ptr)
{
  return bswap_le(*Ptr);
}

template <class T>
static INTMEM_CE T load_be(const T* Ptr)
{
  return bswap_be(*Ptr);
}

template <class T>
static INTMEM_CE void store_le(T* Ptr, T const V)
{
  *Ptr = bswap_le(V);
}

template <class T>
static INTMEM_CE void store_be(T* Ptr, T const V)
{
  *Ptr = bswap_be(V);
}

} // intmem

#endif
