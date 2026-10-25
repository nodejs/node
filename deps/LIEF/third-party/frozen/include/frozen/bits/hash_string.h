#ifndef FROZEN_LETITGO_BITS_HASH_STRING_H
#define FROZEN_LETITGO_BITS_HASH_STRING_H

#include <cstddef>

namespace frozen {

template <typename String>
constexpr std::size_t hash_string(const String& value) {
  std::size_t d = 5381;
  for (const auto& c : value)
    d = d * 33 + static_cast<std::size_t>(c);
  return d;
}

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// With the lowest bits removed, based on experimental setup.
template <typename String>
constexpr std::size_t hash_string(const String& value, std::size_t seed) {
  std::size_t d =  (0x811c9dc5 ^ seed) * static_cast<std::size_t>(0x01000193);
  for (const auto& c : value)
    d = (d ^ static_cast<std::size_t>(c)) * static_cast<std::size_t>(0x01000193);
  return d >> 8 ;
}

} // namespace frozen

#endif // FROZEN_LETITGO_BITS_HASH_STRING_H