#ifndef FROZEN_LETITGO_BITS_ELSA_STD_H
#define FROZEN_LETITGO_BITS_ELSA_STD_H

#include "defines.h"
#include "elsa.h"
#include "hash_string.h"

#ifdef FROZEN_LETITGO_HAS_STRING_VIEW
#include <string_view>
#endif
#include <string>

namespace frozen {

#ifdef FROZEN_LETITGO_HAS_STRING_VIEW

template <typename CharT> struct elsa<std::basic_string_view<CharT>>
{
    constexpr std::size_t operator()(const std::basic_string_view<CharT>& value) const {
        return hash_string(value);
    }
    constexpr std::size_t operator()(const std::basic_string_view<CharT>& value, std::size_t seed) const {
        return hash_string(value, seed);
    }
};

#endif

template <typename CharT> struct elsa<std::basic_string<CharT>>
{
    constexpr std::size_t operator()(const std::basic_string<CharT>& value) const {
        return hash_string(value);
    }
    constexpr std::size_t operator()(const std::basic_string<CharT>& value, std::size_t seed) const {
        return hash_string(value, seed);
    }
};

} // namespace frozen

#endif // FROZEN_LETITGO_BITS_ELSA_STD_H
