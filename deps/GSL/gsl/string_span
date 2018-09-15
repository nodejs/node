///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef GSL_STRING_SPAN_H
#define GSL_STRING_SPAN_H

#include <gsl/gsl_assert> // for Ensures, Expects
#include <gsl/gsl_util>   // for narrow_cast
#include <gsl/span>       // for operator!=, operator==, dynamic_extent
#include <gsl/pointers>   // for not_null

#include <algorithm> // for equal, lexicographical_compare
#include <array>     // for array
#include <cstddef>   // for ptrdiff_t, size_t, nullptr_t
#include <cstdint>   // for PTRDIFF_MAX
#include <cstring>
#include <string>      // for basic_string, allocator, char_traits
#include <type_traits> // for declval, is_convertible, enable_if_t, add_...

#ifdef _MSC_VER
#pragma warning(push)

// Turn MSVC /analyze rules that generate too much noise. TODO: fix in the tool.
#pragma warning(disable : 26446) // TODO: bug in parser - attributes and templates
#pragma warning(disable : 26481) // TODO: suppress does not work inside templates sometimes

#if _MSC_VER < 1910
#pragma push_macro("constexpr")
#define constexpr /*constexpr*/

#endif // _MSC_VER < 1910
#endif // _MSC_VER

namespace gsl
{
//
// czstring and wzstring
//
// These are "tag" typedefs for C-style strings (i.e. null-terminated character arrays)
// that allow static analysis to help find bugs.
//
// There are no additional features/semantics that we can find a way to add inside the
// type system for these types that will not either incur significant runtime costs or
// (sometimes needlessly) break existing programs when introduced.
//

template <typename CharT, std::ptrdiff_t Extent = dynamic_extent>
using basic_zstring = CharT*;

template <std::ptrdiff_t Extent = dynamic_extent>
using czstring = basic_zstring<const char, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using cwzstring = basic_zstring<const wchar_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using cu16zstring = basic_zstring<const char16_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using cu32zstring = basic_zstring<const char32_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using zstring = basic_zstring<char, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using wzstring = basic_zstring<wchar_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using u16zstring = basic_zstring<char16_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using u32zstring = basic_zstring<char32_t, Extent>;

namespace details
{
    template <class CharT>
    std::ptrdiff_t string_length(const CharT* str, std::ptrdiff_t n)
    {
        if (str == nullptr || n <= 0) return 0;

        const span<const CharT> str_span{str, n};

        std::ptrdiff_t len = 0;
        while (len < n && str_span[len]) len++;

        return len;
    }
} // namespace details

//
// ensure_sentinel()
//
// Provides a way to obtain an span from a contiguous sequence
// that ends with a (non-inclusive) sentinel value.
//
// Will fail-fast if sentinel cannot be found before max elements are examined.
//
template <typename T, const T Sentinel>
span<T, dynamic_extent> ensure_sentinel(T* seq, std::ptrdiff_t max = PTRDIFF_MAX)
{
    Ensures(seq != nullptr);

    GSL_SUPPRESS(f.23) // NO-FORMAT: attribute // TODO: false positive // TODO: suppress does not work
    auto cur = seq;
    Ensures(cur != nullptr); // workaround for removing the warning

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute // TODO: suppress does not work
    while ((cur - seq) < max && *cur != Sentinel) ++cur;
    Ensures(*cur == Sentinel);
    return {seq, cur - seq};
}

//
// ensure_z - creates a span for a zero terminated strings.
// Will fail fast if a null-terminator cannot be found before
// the limit of size_type.
//
template <typename CharT>
span<CharT, dynamic_extent> ensure_z(CharT* const& sz, std::ptrdiff_t max = PTRDIFF_MAX)
{
    return ensure_sentinel<CharT, CharT(0)>(sz, max);
}

template <typename CharT, std::size_t N>
span<CharT, dynamic_extent> ensure_z(CharT (&sz)[N])
{
    return ensure_z(&sz[0], narrow_cast<std::ptrdiff_t>(N));
}

template <class Cont>
span<typename std::remove_pointer<typename Cont::pointer>::type, dynamic_extent>
ensure_z(Cont& cont)
{
    return ensure_z(cont.data(), narrow_cast<std::ptrdiff_t>(cont.size()));
}

template <typename CharT, std::ptrdiff_t>
class basic_string_span;

namespace details {
    template <typename T>
    struct is_basic_string_span_oracle : std::false_type
    {
    };

    template <typename CharT, std::ptrdiff_t Extent>
    struct is_basic_string_span_oracle<basic_string_span<CharT, Extent>> : std::true_type
    {
    };

    template <typename T>
    struct is_basic_string_span : is_basic_string_span_oracle<std::remove_cv_t<T>>
    {
    };
} // namespace details

//
// string_span and relatives
//
template <typename CharT, std::ptrdiff_t Extent = dynamic_extent>
class basic_string_span
{
public:
    using element_type = CharT;
    using pointer = std::add_pointer_t<element_type>;
    using reference = std::add_lvalue_reference_t<element_type>;
    using const_reference = std::add_lvalue_reference_t<std::add_const_t<element_type>>;
    using impl_type = span<element_type, Extent>;

    using index_type = typename impl_type::index_type;
    using iterator = typename impl_type::iterator;
    using const_iterator = typename impl_type::const_iterator;
    using reverse_iterator = typename impl_type::reverse_iterator;
    using const_reverse_iterator = typename impl_type::const_reverse_iterator;

    // default (empty)
    constexpr basic_string_span() noexcept = default;

    // copy
    constexpr basic_string_span(const basic_string_span& other) noexcept = default;

    // assign
    constexpr basic_string_span& operator=(const basic_string_span& other) noexcept = default;

    constexpr basic_string_span(pointer ptr, index_type length) : span_(ptr, length) {}
    constexpr basic_string_span(pointer firstElem, pointer lastElem) : span_(firstElem, lastElem) {}

    // From static arrays - if 0-terminated, remove 0 from the view
    // All other containers allow 0s within the length, so we do not remove them
    template <std::size_t N>
    constexpr basic_string_span(element_type (&arr)[N]) : span_(remove_z(arr))
    {}

    template <std::size_t N, class ArrayElementType = std::remove_const_t<element_type>>
    constexpr basic_string_span(std::array<ArrayElementType, N>& arr) noexcept : span_(arr)
    {}

    template <std::size_t N, class ArrayElementType = std::remove_const_t<element_type>>
    constexpr basic_string_span(const std::array<ArrayElementType, N>& arr) noexcept : span_(arr)
    {}

    // Container signature should work for basic_string after C++17 version exists
    template <class Traits, class Allocator>
    // GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute // TODO: parser bug
    constexpr basic_string_span(std::basic_string<element_type, Traits, Allocator>& str)
        : span_(&str[0], narrow_cast<std::ptrdiff_t>(str.length()))
    {}

    template <class Traits, class Allocator>
    constexpr basic_string_span(const std::basic_string<element_type, Traits, Allocator>& str)
        : span_(&str[0], str.length())
    {}

    // from containers. Containers must have a pointer type and data() function signatures
    template <class Container,
              class = std::enable_if_t<
                  !details::is_basic_string_span<Container>::value &&
                  std::is_convertible<typename Container::pointer, pointer>::value &&
                  std::is_convertible<typename Container::pointer,
                                      decltype(std::declval<Container>().data())>::value>>
    constexpr basic_string_span(Container& cont) : span_(cont)
    {}

    template <class Container,
              class = std::enable_if_t<
                  !details::is_basic_string_span<Container>::value &&
                  std::is_convertible<typename Container::pointer, pointer>::value &&
                  std::is_convertible<typename Container::pointer,
                                      decltype(std::declval<Container>().data())>::value>>
    constexpr basic_string_span(const Container& cont) : span_(cont)
    {}

    // from string_span
    template <
        class OtherValueType, std::ptrdiff_t OtherExtent,
        class = std::enable_if_t<std::is_convertible<
            typename basic_string_span<OtherValueType, OtherExtent>::impl_type, impl_type>::value>>
    constexpr basic_string_span(basic_string_span<OtherValueType, OtherExtent> other)
        : span_(other.data(), other.length())
    {}

    template <index_type Count>
    constexpr basic_string_span<element_type, Count> first() const
    {
        return {span_.template first<Count>()};
    }

    constexpr basic_string_span<element_type, dynamic_extent> first(index_type count) const
    {
        return {span_.first(count)};
    }

    template <index_type Count>
    constexpr basic_string_span<element_type, Count> last() const
    {
        return {span_.template last<Count>()};
    }

    constexpr basic_string_span<element_type, dynamic_extent> last(index_type count) const
    {
        return {span_.last(count)};
    }

    template <index_type Offset, index_type Count>
    constexpr basic_string_span<element_type, Count> subspan() const
    {
        return {span_.template subspan<Offset, Count>()};
    }

    constexpr basic_string_span<element_type, dynamic_extent>
    subspan(index_type offset, index_type count = dynamic_extent) const
    {
        return {span_.subspan(offset, count)};
    }

    constexpr reference operator[](index_type idx) const { return span_[idx]; }
    constexpr reference operator()(index_type idx) const { return span_[idx]; }

    constexpr pointer data() const { return span_.data(); }

    constexpr index_type length() const noexcept { return span_.size(); }
    constexpr index_type size() const noexcept { return span_.size(); }
    constexpr index_type size_bytes() const noexcept { return span_.size_bytes(); }
    constexpr index_type length_bytes() const noexcept { return span_.length_bytes(); }
    constexpr bool empty() const noexcept { return size() == 0; }

    constexpr iterator begin() const noexcept { return span_.begin(); }
    constexpr iterator end() const noexcept { return span_.end(); }

    constexpr const_iterator cbegin() const noexcept { return span_.cbegin(); }
    constexpr const_iterator cend() const noexcept { return span_.cend(); }

    constexpr reverse_iterator rbegin() const noexcept { return span_.rbegin(); }
    constexpr reverse_iterator rend() const noexcept { return span_.rend(); }

    constexpr const_reverse_iterator crbegin() const noexcept { return span_.crbegin(); }
    constexpr const_reverse_iterator crend() const noexcept { return span_.crend(); }

private:
    static impl_type remove_z(pointer const& sz, std::ptrdiff_t max)
    {
        return {sz, details::string_length(sz, max)};
    }

    template <std::size_t N>
    static impl_type remove_z(element_type (&sz)[N])
    {
        return remove_z(&sz[0], narrow_cast<std::ptrdiff_t>(N));
    }

    impl_type span_;
};

template <std::ptrdiff_t Extent = dynamic_extent>
using string_span = basic_string_span<char, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using cstring_span = basic_string_span<const char, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using wstring_span = basic_string_span<wchar_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using cwstring_span = basic_string_span<const wchar_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using u16string_span = basic_string_span<char16_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using cu16string_span = basic_string_span<const char16_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using u32string_span = basic_string_span<char32_t, Extent>;

template <std::ptrdiff_t Extent = dynamic_extent>
using cu32string_span = basic_string_span<const char32_t, Extent>;

//
// to_string() allow (explicit) conversions from string_span to string
//

template <typename CharT, std::ptrdiff_t Extent>
std::basic_string<typename std::remove_const<CharT>::type>
to_string(basic_string_span<CharT, Extent> view)
{
    return {view.data(), narrow_cast<std::size_t>(view.length())};
}

template <typename CharT, typename Traits = typename std::char_traits<CharT>,
          typename Allocator = std::allocator<CharT>, typename gCharT, std::ptrdiff_t Extent>
std::basic_string<CharT, Traits, Allocator> to_basic_string(basic_string_span<gCharT, Extent> view)
{
    return {view.data(), narrow_cast<std::size_t>(view.length())};
}

template <class ElementType, std::ptrdiff_t Extent>
basic_string_span<const byte, details::calculate_byte_size<ElementType, Extent>::value>
as_bytes(basic_string_span<ElementType, Extent> s) noexcept
{
    GSL_SUPPRESS(type.1) // NO-FORMAT: attribute
    return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
}

template <class ElementType, std::ptrdiff_t Extent,
          class = std::enable_if_t<!std::is_const<ElementType>::value>>
basic_string_span<byte, details::calculate_byte_size<ElementType, Extent>::value>
as_writeable_bytes(basic_string_span<ElementType, Extent> s) noexcept
{
    GSL_SUPPRESS(type.1) // NO-FORMAT: attribute
    return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
}

// zero-terminated string span, used to convert
// zero-terminated spans to legacy strings
template <typename CharT, std::ptrdiff_t Extent = dynamic_extent>
class basic_zstring_span {
public:
    using value_type = CharT;
    using const_value_type = std::add_const_t<CharT>;

    using pointer = std::add_pointer_t<value_type>;
    using const_pointer = std::add_pointer_t<const_value_type>;

    using zstring_type = basic_zstring<value_type, Extent>;
    using const_zstring_type = basic_zstring<const_value_type, Extent>;

    using impl_type = span<value_type, Extent>;
    using string_span_type = basic_string_span<value_type, Extent>;

    constexpr basic_zstring_span(impl_type s) : span_(s)
    {
        // expects a zero-terminated span
        Expects(s[s.size() - 1] == '\0');
    }

    // copy
    constexpr basic_zstring_span(const basic_zstring_span& other) = default;

    // move
    constexpr basic_zstring_span(basic_zstring_span&& other) = default;

    // assign
    constexpr basic_zstring_span& operator=(const basic_zstring_span& other) = default;

    // move assign
    constexpr basic_zstring_span& operator=(basic_zstring_span&& other) = default;

    constexpr bool empty() const noexcept { return span_.size() == 0; }

    constexpr string_span_type as_string_span() const noexcept
    {
        const auto sz = span_.size();
        return {span_.data(), sz > 1 ? sz - 1 : 0};
    }
    constexpr string_span_type ensure_z() const { return gsl::ensure_z(span_); }

    constexpr const_zstring_type assume_z() const noexcept { return span_.data(); }

private:
    impl_type span_;
};

template <std::ptrdiff_t Max = dynamic_extent>
using zstring_span = basic_zstring_span<char, Max>;

template <std::ptrdiff_t Max = dynamic_extent>
using wzstring_span = basic_zstring_span<wchar_t, Max>;

template <std::ptrdiff_t Max = dynamic_extent>
using u16zstring_span = basic_zstring_span<char16_t, Max>;

template <std::ptrdiff_t Max = dynamic_extent>
using u32zstring_span = basic_zstring_span<char32_t, Max>;

template <std::ptrdiff_t Max = dynamic_extent>
using czstring_span = basic_zstring_span<const char, Max>;

template <std::ptrdiff_t Max = dynamic_extent>
using cwzstring_span = basic_zstring_span<const wchar_t, Max>;

template <std::ptrdiff_t Max = dynamic_extent>
using cu16zstring_span = basic_zstring_span<const char16_t, Max>;

template <std::ptrdiff_t Max = dynamic_extent>
using cu32zstring_span = basic_zstring_span<const char32_t, Max>;

// operator ==
template <class CharT, std::ptrdiff_t Extent, class T,
          class = std::enable_if_t<
              details::is_basic_string_span<T>::value ||
              std::is_convertible<T, gsl::basic_string_span<std::add_const_t<CharT>>>::value>>
bool operator==(const gsl::basic_string_span<CharT, Extent>& one, const T& other)
{
    const gsl::basic_string_span<std::add_const_t<CharT>> tmp(other);
    return std::equal(one.begin(), one.end(), tmp.begin(), tmp.end());
}

template <class CharT, std::ptrdiff_t Extent, class T,
          class = std::enable_if_t<
              !details::is_basic_string_span<T>::value &&
              std::is_convertible<T, gsl::basic_string_span<std::add_const_t<CharT>>>::value>>
bool operator==(const T& one, const gsl::basic_string_span<CharT, Extent>& other)
{
    const gsl::basic_string_span<std::add_const_t<CharT>> tmp(one);
    return std::equal(tmp.begin(), tmp.end(), other.begin(), other.end());
}

// operator !=
template <typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
          typename = std::enable_if_t<std::is_convertible<
              T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value>>
bool operator!=(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    return !(one == other);
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename = std::enable_if_t<
        std::is_convertible<T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value &&
        !gsl::details::is_basic_string_span<T>::value>>
bool operator!=(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    return !(one == other);
}

// operator<
template <typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
          typename = std::enable_if_t<std::is_convertible<
              T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value>>
bool operator<(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    const gsl::basic_string_span<std::add_const_t<CharT>, Extent> tmp(other);
    return std::lexicographical_compare(one.begin(), one.end(), tmp.begin(), tmp.end());
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename = std::enable_if_t<
        std::is_convertible<T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value &&
        !gsl::details::is_basic_string_span<T>::value>>
bool operator<(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    gsl::basic_string_span<std::add_const_t<CharT>, Extent> tmp(one);
    return std::lexicographical_compare(tmp.begin(), tmp.end(), other.begin(), other.end());
}

#ifndef _MSC_VER

// VS treats temp and const containers as convertible to basic_string_span,
// so the cases below are already covered by the previous operators

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename DataType = typename T::value_type,
    typename = std::enable_if_t<
        !gsl::details::is_span<T>::value && !gsl::details::is_basic_string_span<T>::value &&
        std::is_convertible<DataType*, CharT*>::value &&
        std::is_same<std::decay_t<decltype(std::declval<T>().size(), *std::declval<T>().data())>,
                     DataType>::value>>
bool operator<(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    gsl::basic_string_span<std::add_const_t<CharT>, Extent> tmp(other);
    return std::lexicographical_compare(one.begin(), one.end(), tmp.begin(), tmp.end());
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename DataType = typename T::value_type,
    typename = std::enable_if_t<
        !gsl::details::is_span<T>::value && !gsl::details::is_basic_string_span<T>::value &&
        std::is_convertible<DataType*, CharT*>::value &&
        std::is_same<std::decay_t<decltype(std::declval<T>().size(), *std::declval<T>().data())>,
                     DataType>::value>>
bool operator<(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    gsl::basic_string_span<std::add_const_t<CharT>, Extent> tmp(one);
    return std::lexicographical_compare(tmp.begin(), tmp.end(), other.begin(), other.end());
}
#endif

// operator <=
template <typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
          typename = std::enable_if_t<std::is_convertible<
              T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value>>
bool operator<=(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    return !(other < one);
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename = std::enable_if_t<
        std::is_convertible<T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value &&
        !gsl::details::is_basic_string_span<T>::value>>
bool operator<=(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    return !(other < one);
}

#ifndef _MSC_VER

// VS treats temp and const containers as convertible to basic_string_span,
// so the cases below are already covered by the previous operators

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename DataType = typename T::value_type,
    typename = std::enable_if_t<
        !gsl::details::is_span<T>::value && !gsl::details::is_basic_string_span<T>::value &&
        std::is_convertible<DataType*, CharT*>::value &&
        std::is_same<std::decay_t<decltype(std::declval<T>().size(), *std::declval<T>().data())>,
                     DataType>::value>>
bool operator<=(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    return !(other < one);
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename DataType = typename T::value_type,
    typename = std::enable_if_t<
        !gsl::details::is_span<T>::value && !gsl::details::is_basic_string_span<T>::value &&
        std::is_convertible<DataType*, CharT*>::value &&
        std::is_same<std::decay_t<decltype(std::declval<T>().size(), *std::declval<T>().data())>,
                     DataType>::value>>
bool operator<=(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    return !(other < one);
}
#endif

// operator>
template <typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
          typename = std::enable_if_t<std::is_convertible<
              T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value>>
bool operator>(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    return other < one;
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename = std::enable_if_t<
        std::is_convertible<T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value &&
        !gsl::details::is_basic_string_span<T>::value>>
bool operator>(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    return other < one;
}

#ifndef _MSC_VER

// VS treats temp and const containers as convertible to basic_string_span,
// so the cases below are already covered by the previous operators

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename DataType = typename T::value_type,
    typename = std::enable_if_t<
        !gsl::details::is_span<T>::value && !gsl::details::is_basic_string_span<T>::value &&
        std::is_convertible<DataType*, CharT*>::value &&
        std::is_same<std::decay_t<decltype(std::declval<T>().size(), *std::declval<T>().data())>,
                     DataType>::value>>
bool operator>(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    return other < one;
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename DataType = typename T::value_type,
    typename = std::enable_if_t<
        !gsl::details::is_span<T>::value && !gsl::details::is_basic_string_span<T>::value &&
        std::is_convertible<DataType*, CharT*>::value &&
        std::is_same<std::decay_t<decltype(std::declval<T>().size(), *std::declval<T>().data())>,
                     DataType>::value>>
bool operator>(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    return other < one;
}
#endif

// operator >=
template <typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
          typename = std::enable_if_t<std::is_convertible<
              T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value>>
bool operator>=(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    return !(one < other);
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename = std::enable_if_t<
        std::is_convertible<T, gsl::basic_string_span<std::add_const_t<CharT>, Extent>>::value &&
        !gsl::details::is_basic_string_span<T>::value>>
bool operator>=(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    return !(one < other);
}

#ifndef _MSC_VER

// VS treats temp and const containers as convertible to basic_string_span,
// so the cases below are already covered by the previous operators

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename DataType = typename T::value_type,
    typename = std::enable_if_t<
        !gsl::details::is_span<T>::value && !gsl::details::is_basic_string_span<T>::value &&
        std::is_convertible<DataType*, CharT*>::value &&
        std::is_same<std::decay_t<decltype(std::declval<T>().size(), *std::declval<T>().data())>,
                     DataType>::value>>
bool operator>=(gsl::basic_string_span<CharT, Extent> one, const T& other)
{
    return !(one < other);
}

template <
    typename CharT, std::ptrdiff_t Extent = gsl::dynamic_extent, typename T,
    typename DataType = typename T::value_type,
    typename = std::enable_if_t<
        !gsl::details::is_span<T>::value && !gsl::details::is_basic_string_span<T>::value &&
        std::is_convertible<DataType*, CharT*>::value &&
        std::is_same<std::decay_t<decltype(std::declval<T>().size(), *std::declval<T>().data())>,
                     DataType>::value>>
bool operator>=(const T& one, gsl::basic_string_span<CharT, Extent> other)
{
    return !(one < other);
}
#endif
} // namespace gsl

#ifdef _MSC_VER
#pragma warning(pop)

#if _MSC_VER < 1910
#undef constexpr
#pragma pop_macro("constexpr")

#endif // _MSC_VER < 1910
#endif // _MSC_VER

#endif // GSL_STRING_SPAN_H
