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

#ifndef GSL_UTIL_H
#define GSL_UTIL_H

#include <gsl/gsl_assert> // for Expects

#include <array>
#include <cstddef>          // for ptrdiff_t, size_t
#include <exception>        // for exception
#include <initializer_list> // for initializer_list
#include <type_traits>      // for is_signed, integral_constant
#include <utility>          // for forward

#if defined(_MSC_VER)

#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant

#if _MSC_VER < 1910
#pragma push_macro("constexpr")
#define constexpr /*constexpr*/
#endif            // _MSC_VER < 1910
#endif            // _MSC_VER

namespace gsl
{
//
// GSL.util: utilities
//

// index type for all container indexes/subscripts/sizes
using index = std::ptrdiff_t;

// final_action allows you to ensure something gets run at the end of a scope
template <class F>
class final_action
{
public:
    explicit final_action(F f) noexcept : f_(std::move(f)) {}

    final_action(final_action&& other) noexcept : f_(std::move(other.f_)), invoke_(other.invoke_)
    {
        other.invoke_ = false;
    }

    final_action(const final_action&) = delete;
    final_action& operator=(const final_action&) = delete;
    final_action& operator=(final_action&&) = delete;

    GSL_SUPPRESS(f.6) // NO-FORMAT: attribute // terminate if throws
    ~final_action() noexcept
    {
        if (invoke_) f_();
    }

private:
    F f_;
    bool invoke_{true};
};

// finally() - convenience function to generate a final_action
template <class F>
final_action<F> finally(const F& f) noexcept
{
    return final_action<F>(f);
}

template <class F>
final_action<F> finally(F&& f) noexcept
{
    return final_action<F>(std::forward<F>(f));
}

// narrow_cast(): a searchable way to do narrowing casts of values
template <class T, class U>
GSL_SUPPRESS(type.1) // NO-FORMAT: attribute
constexpr T narrow_cast(U&& u) noexcept
{
    return static_cast<T>(std::forward<U>(u));
}

struct narrowing_error : public std::exception
{
};

namespace details
{
    template <class T, class U>
    struct is_same_signedness
        : public std::integral_constant<bool, std::is_signed<T>::value == std::is_signed<U>::value>
    {
    };
} // namespace details

// narrow() : a checked version of narrow_cast() that throws if the cast changed the value
template <class T, class U>
GSL_SUPPRESS(type.1) // NO-FORMAT: attribute
GSL_SUPPRESS(f.6) // NO-FORMAT: attribute // TODO: MSVC /analyze does not recognise noexcept(false)
T narrow(U u) noexcept(false)
{
    T t = narrow_cast<T>(u);
    if (static_cast<U>(t) != u) gsl::details::throw_exception(narrowing_error());
    if (!details::is_same_signedness<T, U>::value && ((t < T{}) != (u < U{})))
        gsl::details::throw_exception(narrowing_error());
    return t;
}

//
// at() - Bounds-checked way of accessing builtin arrays, std::array, std::vector
//
template <class T, std::size_t N>
GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
GSL_SUPPRESS(bounds.2) // NO-FORMAT: attribute
constexpr T& at(T (&arr)[N], const index i)
{
    Expects(i >= 0 && i < narrow_cast<index>(N));
    return arr[narrow_cast<std::size_t>(i)];
}

template <class Cont>
GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
GSL_SUPPRESS(bounds.2) // NO-FORMAT: attribute
constexpr auto at(Cont& cont, const index i) -> decltype(cont[cont.size()])
{
    Expects(i >= 0 && i < narrow_cast<index>(cont.size()));
    using size_type = decltype(cont.size());
    return cont[narrow_cast<size_type>(i)];
}

template <class T>
GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
constexpr T at(const std::initializer_list<T> cont, const index i)
{
    Expects(i >= 0 && i < narrow_cast<index>(cont.size()));
    return *(cont.begin() + i);
}

} // namespace gsl

#if defined(_MSC_VER)
#if _MSC_VER < 1910
#undef constexpr
#pragma pop_macro("constexpr")

#endif // _MSC_VER < 1910

#pragma warning(pop)

#endif // _MSC_VER

#endif // GSL_UTIL_H
