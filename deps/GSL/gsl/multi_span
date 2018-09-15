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

#ifndef GSL_MULTI_SPAN_H
#define GSL_MULTI_SPAN_H

#include <gsl/gsl_assert> // for Expects
#include <gsl/gsl_byte>   // for byte
#include <gsl/gsl_util>   // for narrow_cast

#include <algorithm> // for transform, lexicographical_compare
#include <array>     // for array
#include <cassert>
#include <cstddef>          // for ptrdiff_t, size_t, nullptr_t
#include <cstdint>          // for PTRDIFF_MAX
#include <functional>       // for divides, multiplies, minus, negate, plus
#include <initializer_list> // for initializer_list
#include <iterator>         // for iterator, random_access_iterator_tag
#include <limits>           // for numeric_limits
#include <new>
#include <numeric>
#include <stdexcept>
#include <string>      // for basic_string
#include <type_traits> // for enable_if_t, remove_cv_t, is_same, is_co...
#include <utility>

#ifdef _MSC_VER

// turn off some warnings that are noisy about our Expects statements
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant
#pragma warning(disable : 4702) // unreachable code

// Turn MSVC /analyze rules that generate too much noise. TODO: fix in the tool.
#pragma warning(disable : 26495) // uninitalized member when constructor calls constructor
#pragma warning(disable : 26473) // in some instantiations we cast to the same type
#pragma warning(disable : 26490) // TODO: bug in parser - attributes and templates
#pragma warning(disable : 26465) // TODO: bug - suppression does not work on template functions

#if _MSC_VER < 1910
#pragma push_macro("constexpr")
#define constexpr /*constexpr*/

#endif                          // _MSC_VER < 1910
#endif                          // _MSC_VER

// GCC 7 does not like the signed unsigned missmatch (size_t ptrdiff_t)
// While there is a conversion from signed to unsigned, it happens at
// compiletime, so the compiler wouldn't have to warn indiscriminently, but
// could check if the source value actually doesn't fit into the target type
// and only warn in those cases.
#if __GNUC__ > 6
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

namespace gsl
{

/*
** begin definitions of index and bounds
*/
namespace details
{
    template <typename SizeType>
    struct SizeTypeTraits
    {
        static const SizeType max_value = std::numeric_limits<SizeType>::max();
    };

    template <typename... Ts>
    class are_integral : public std::integral_constant<bool, true>
    {
    };

    template <typename T, typename... Ts>
    class are_integral<T, Ts...>
        : public std::integral_constant<bool,
                                        std::is_integral<T>::value && are_integral<Ts...>::value>
    {
    };
} // namespace details

template <std::size_t Rank>
class multi_span_index final
{
    static_assert(Rank > 0, "Rank must be greater than 0!");

    template <std::size_t OtherRank>
    friend class multi_span_index;

public:
    static const std::size_t rank = Rank;
    using value_type = std::ptrdiff_t;
    using size_type = value_type;
    using reference = std::add_lvalue_reference_t<value_type>;
    using const_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;

    constexpr multi_span_index() noexcept {}

    constexpr multi_span_index(const value_type (&values)[Rank]) noexcept
    {
        GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
        GSL_SUPPRESS(bounds.3) // NO-FORMAT: attribute
        std::copy(values, values + Rank, elems);
    }

    template <typename... Ts, typename = std::enable_if_t<(sizeof...(Ts) == Rank) &&
                                                          details::are_integral<Ts...>::value>>
    constexpr multi_span_index(Ts... ds) noexcept : elems{narrow_cast<value_type>(ds)...}
    {}

    constexpr multi_span_index(const multi_span_index& other) noexcept = default;

    constexpr multi_span_index& operator=(const multi_span_index& rhs) noexcept = default;

    // Preconditions: component_idx < rank
    GSL_SUPPRESS(bounds.2) // NO-FORMAT: attribute
    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr reference operator[](std::size_t component_idx)
    {
        Expects(component_idx < Rank); // Component index must be less than rank
        return elems[component_idx];
    }

    // Preconditions: component_idx < rank
    GSL_SUPPRESS(bounds.2) // NO-FORMAT: attribute
    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr const_reference operator[](std::size_t component_idx) const 
    {
        Expects(component_idx < Rank); // Component index must be less than rank
        return elems[component_idx];
    }

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    GSL_SUPPRESS(bounds.3) // NO-FORMAT: attribute
    constexpr bool operator==(const multi_span_index& rhs) const
    {
        return std::equal(elems, elems + rank, rhs.elems);
    }

    constexpr bool operator!=(const multi_span_index& rhs) const
    {
        return !(*this == rhs);
    }

    constexpr multi_span_index operator+() const noexcept { return *this; }

    constexpr multi_span_index operator-() const
    {
        multi_span_index ret = *this;
        std::transform(ret, ret + rank, ret, std::negate<value_type>{});
        return ret;
    }

    constexpr multi_span_index operator+(const multi_span_index& rhs) const
    {
        multi_span_index ret = *this;
        ret += rhs;
        return ret;
    }

    constexpr multi_span_index operator-(const multi_span_index& rhs) const
    {
        multi_span_index ret = *this;
        ret -= rhs;
        return ret;
    }

    constexpr multi_span_index& operator+=(const multi_span_index& rhs)
    {
        GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
        GSL_SUPPRESS(bounds.3) // NO-FORMAT: attribute
        std::transform(elems, elems + rank, rhs.elems, elems,
                       std::plus<value_type>{});
        return *this;
    }

    constexpr multi_span_index& operator-=(const multi_span_index& rhs)
    {
        GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
        GSL_SUPPRESS(bounds.3) // NO-FORMAT: attribute
        std::transform(elems, elems + rank, rhs.elems, elems, std::minus<value_type>{});
        return *this;
    }

    constexpr multi_span_index operator*(value_type v) const
    {
        multi_span_index ret = *this;
        ret *= v;
        return ret;
    }

    constexpr multi_span_index operator/(value_type v) const
    {
        multi_span_index ret = *this;
        ret /= v;
        return ret;
    }

    friend constexpr multi_span_index operator*(value_type v, const multi_span_index& rhs)
    {
        return rhs * v;
    }

    constexpr multi_span_index& operator*=(value_type v)
    {
        GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
        GSL_SUPPRESS(bounds.3) // NO-FORMAT: attribute
        std::transform(elems, elems + rank, elems,
                       [v](value_type x) { return std::multiplies<value_type>{}(x, v); });
        return *this;
    }

    constexpr multi_span_index& operator/=(value_type v)
    {
        GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
        GSL_SUPPRESS(bounds.3) // NO-FORMAT: attribute
        std::transform(elems, elems + rank, elems,
                       [v](value_type x) { return std::divides<value_type>{}(x, v); });
        return *this;
    }

private:
    value_type elems[Rank] = {};
};

#if !defined(_MSC_VER) || _MSC_VER >= 1910

struct static_bounds_dynamic_range_t
{
    template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
    constexpr operator T() const noexcept
    {
        return narrow_cast<T>(-1);
    }
};

constexpr bool operator==(static_bounds_dynamic_range_t, static_bounds_dynamic_range_t) noexcept
{
    return true;
}

constexpr bool operator!=(static_bounds_dynamic_range_t, static_bounds_dynamic_range_t) noexcept
{
    return false;
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr bool operator==(static_bounds_dynamic_range_t, T other) noexcept
{
    return narrow_cast<T>(-1) == other;
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr bool operator==(T left, static_bounds_dynamic_range_t right) noexcept
{
    return right == left;
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr bool operator!=(static_bounds_dynamic_range_t, T other) noexcept
{
    return narrow_cast<T>(-1) != other;
}

template <typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
constexpr bool operator!=(T left, static_bounds_dynamic_range_t right) noexcept
{
    return right != left;
}

constexpr static_bounds_dynamic_range_t dynamic_range{};
#else
const std::ptrdiff_t dynamic_range = -1;
#endif

struct generalized_mapping_tag
{
};
struct contiguous_mapping_tag : generalized_mapping_tag
{
};

namespace details
{

    template <std::ptrdiff_t Left, std::ptrdiff_t Right>
    struct LessThan
    {
        static const bool value = Left < Right;
    };

    template <std::ptrdiff_t... Ranges>
    struct BoundsRanges
    {
        using size_type = std::ptrdiff_t;
        static const size_type Depth = 0;
        static const size_type DynamicNum = 0;
        static const size_type CurrentRange = 1;
        static const size_type TotalSize = 1;

        // TODO : following signature is for work around VS bug
        template <typename OtherRange>
        constexpr BoundsRanges(const OtherRange&, bool /* firstLevel */)
        {}

        constexpr BoundsRanges(const std::ptrdiff_t* const) {}
        constexpr BoundsRanges() noexcept = default;

        template <typename T, std::size_t Dim>
        constexpr void serialize(T&) const
        {}

        template <typename T, std::size_t Dim>
        constexpr size_type linearize(const T&) const
        {
            return 0;
        }

        template <typename T, std::size_t Dim>
        constexpr size_type contains(const T&) const
        {
            return -1;
        }

        constexpr size_type elementNum(std::size_t) const noexcept { return 0; }

        constexpr size_type totalSize() const noexcept { return TotalSize; }

        constexpr bool operator==(const BoundsRanges&) const noexcept { return true; }
    };

    template <std::ptrdiff_t... RestRanges>
    struct BoundsRanges<dynamic_range, RestRanges...> : BoundsRanges<RestRanges...>
    {
        using Base = BoundsRanges<RestRanges...>;
        using size_type = std::ptrdiff_t;
        static const std::size_t Depth = Base::Depth + 1;
        static const std::size_t DynamicNum = Base::DynamicNum + 1;
        static const size_type CurrentRange = dynamic_range;
        static const size_type TotalSize = dynamic_range;

    private:
        size_type m_bound;

    public:
        GSL_SUPPRESS(f.23) // NO-FORMAT: attribute // this pointer type is cannot be assigned nullptr - issue in not_null
        GSL_SUPPRESS(bounds.1)  // NO-FORMAT: attribute
        constexpr BoundsRanges(const std::ptrdiff_t* const arr)
            : Base(arr + 1), m_bound(*arr * this->Base::totalSize())
        {
            Expects(0 <= *arr);
        }

        constexpr BoundsRanges() noexcept : m_bound(0) {}

        template <std::ptrdiff_t OtherRange, std::ptrdiff_t... RestOtherRanges>
        constexpr BoundsRanges(const BoundsRanges<OtherRange, RestOtherRanges...>& other,
                     bool /* firstLevel */ = true)
            : Base(static_cast<const BoundsRanges<RestOtherRanges...>&>(other), false)
            , m_bound(other.totalSize())
        {}

        template <typename T, std::size_t Dim = 0>
        constexpr void serialize(T& arr) const
        {
            arr[Dim] = elementNum();
            this->Base::template serialize<T, Dim + 1>(arr);
        }

        template <typename T, std::size_t Dim = 0>
        GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
        constexpr size_type linearize(const T& arr) const
        {
            const size_type index = this->Base::totalSize() * arr[Dim];
            Expects(index < m_bound);
            return index + this->Base::template linearize<T, Dim + 1>(arr);
        }

        template <typename T, std::size_t Dim = 0>
        constexpr size_type contains(const T& arr) const
        {
            const ptrdiff_t last = this->Base::template contains<T, Dim + 1>(arr);
            if (last == -1) return -1;
            const ptrdiff_t cur = this->Base::totalSize() * arr[Dim];
            return cur < m_bound ? cur + last : -1;
        }

        GSL_SUPPRESS(c.128) // NO-FORMAT: attribute // no pointers to BoundsRanges should be ever used
        constexpr size_type totalSize() const noexcept
        {
            return m_bound;
        }

        GSL_SUPPRESS(c.128) // NO-FORMAT: attribute // no pointers to BoundsRanges should be ever used
        constexpr size_type elementNum() const noexcept
        {
            return totalSize() / this->Base::totalSize();
        }

        constexpr size_type elementNum(std::size_t dim) const noexcept
        {
            if (dim > 0)
                return this->Base::elementNum(dim - 1);
            else
                return elementNum();
        }

        constexpr bool operator==(const BoundsRanges& rhs) const noexcept
        {
            return m_bound == rhs.m_bound &&
                   static_cast<const Base&>(*this) == static_cast<const Base&>(rhs);
        }
    };

    template <std::ptrdiff_t CurRange, std::ptrdiff_t... RestRanges>
    struct BoundsRanges<CurRange, RestRanges...> : BoundsRanges<RestRanges...>
    {
        using Base = BoundsRanges<RestRanges...>;
        using size_type = std::ptrdiff_t;
        static const std::size_t Depth = Base::Depth + 1;
        static const std::size_t DynamicNum = Base::DynamicNum;
        static const size_type CurrentRange = CurRange;
        static const size_type TotalSize =
            Base::TotalSize == dynamic_range ? dynamic_range : CurrentRange * Base::TotalSize;

        constexpr BoundsRanges(const std::ptrdiff_t* const arr) : Base(arr) {}
        constexpr BoundsRanges() = default;

        template <std::ptrdiff_t OtherRange, std::ptrdiff_t... RestOtherRanges>
        constexpr BoundsRanges(const BoundsRanges<OtherRange, RestOtherRanges...>& other,
                     bool firstLevel = true)
            : Base(static_cast<const BoundsRanges<RestOtherRanges...>&>(other), false)
        {
            GSL_SUPPRESS(type.4) // NO-FORMAT: attribute // TODO: false positive
            (void) firstLevel;
        }

        template <typename T, std::size_t Dim = 0>
        constexpr void serialize(T& arr) const
        {
            arr[Dim] = elementNum();
            this->Base::template serialize<T, Dim + 1>(arr);
        }

        template <typename T, std::size_t Dim = 0>
        constexpr size_type linearize(const T& arr) const
        {
            GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
            Expects(arr[Dim] >= 0 && arr[Dim] < CurrentRange); // Index is out of range
            GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
            const ptrdiff_t d = arr[Dim];
            return this->Base::totalSize() * d +
                   this->Base::template linearize<T, Dim + 1>(arr);
        }

        template <typename T, std::size_t Dim = 0>
        constexpr size_type contains(const T& arr) const
        {
            if (arr[Dim] >= CurrentRange) return -1;
            const size_type last = this->Base::template contains<T, Dim + 1>(arr);
            if (last == -1) return -1;
            return this->Base::totalSize() * arr[Dim] + last;
        }

        GSL_SUPPRESS(c.128) // NO-FORMAT: attribute // no pointers to BoundsRanges should be ever used
        constexpr size_type totalSize() const noexcept
        {
            return CurrentRange * this->Base::totalSize();
        }

        GSL_SUPPRESS(c.128) // NO-FORMAT: attribute // no pointers to BoundsRanges should be ever used
        constexpr size_type elementNum() const noexcept
        {
            return CurrentRange;
        }

        GSL_SUPPRESS(c.128) // NO-FORMAT: attribute // no pointers to BoundsRanges should be ever used
        constexpr size_type elementNum(std::size_t dim) const noexcept
        {
            if (dim > 0)
                return this->Base::elementNum(dim - 1);
            else
                return elementNum();
        }

        constexpr bool operator==(const BoundsRanges& rhs) const noexcept
        {
            return static_cast<const Base&>(*this) == static_cast<const Base&>(rhs);
        }
    };

    template <typename SourceType, typename TargetType>
    struct BoundsRangeConvertible
        : public std::integral_constant<bool, (SourceType::TotalSize >= TargetType::TotalSize ||
                                               TargetType::TotalSize == dynamic_range ||
                                               SourceType::TotalSize == dynamic_range ||
                                               TargetType::TotalSize == 0)>
    {
    };

    template <typename TypeChain>
    struct TypeListIndexer
    {
        const TypeChain& obj_;
        TypeListIndexer(const TypeChain& obj) : obj_(obj) {}

        template <std::size_t N>
        const TypeChain& getObj(std::true_type)
        {
            return obj_;
        }

        template <std::size_t N, typename MyChain = TypeChain,
                  typename MyBase = typename MyChain::Base>
        auto getObj(std::false_type)
            -> decltype(TypeListIndexer<MyBase>(static_cast<const MyBase&>(obj_)).template get<N>())
        {
            return TypeListIndexer<MyBase>(static_cast<const MyBase&>(obj_)).template get<N>();
        }

        template <std::size_t N>
        auto get() -> decltype(getObj<N - 1>(std::integral_constant<bool, N == 0>()))
        {
            return getObj<N - 1>(std::integral_constant<bool, N == 0>());
        }
    };

    template <typename TypeChain>
    TypeListIndexer<TypeChain> createTypeListIndexer(const TypeChain& obj)
    {
        return TypeListIndexer<TypeChain>(obj);
    }

    template <std::size_t Rank, bool Enabled = (Rank > 1),
              typename Ret = std::enable_if_t<Enabled, multi_span_index<Rank - 1>>>
    constexpr Ret shift_left(const multi_span_index<Rank>& other) noexcept
    {
        Ret ret{};
        for (std::size_t i = 0; i < Rank - 1; ++i)
        {
            GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
                ret[i] = other[i + 1];
        }
        return ret;
    }
} // namespace details

template <typename IndexType>
class bounds_iterator;

template <std::ptrdiff_t... Ranges>
class static_bounds
{
public:
    static_bounds(const details::BoundsRanges<Ranges...>&) {}
};

template <std::ptrdiff_t FirstRange, std::ptrdiff_t... RestRanges>
class static_bounds<FirstRange, RestRanges...>
{
    using MyRanges = details::BoundsRanges<FirstRange, RestRanges...>;

    MyRanges m_ranges;
    constexpr static_bounds(const MyRanges& range) noexcept : m_ranges(range) {}

    template <std::ptrdiff_t... OtherRanges>
    friend class static_bounds;

public:
    static const std::size_t rank = MyRanges::Depth;
    static const std::size_t dynamic_rank = MyRanges::DynamicNum;
    static const std::ptrdiff_t static_size = MyRanges::TotalSize;

    using size_type = std::ptrdiff_t;
    using index_type = multi_span_index<rank>;
    using const_index_type = std::add_const_t<index_type>;
    using iterator = bounds_iterator<const_index_type>;
    using const_iterator = bounds_iterator<const_index_type>;
    using difference_type = std::ptrdiff_t;
    using sliced_type = static_bounds<RestRanges...>;
    using mapping_type = contiguous_mapping_tag;

    constexpr static_bounds(const static_bounds&) noexcept = default;
    constexpr static_bounds() /*noexcept*/ = default;

    template <typename SourceType, typename TargetType, std::size_t Rank>
    struct BoundsRangeConvertible2;

    template <std::size_t Rank, typename SourceType, typename TargetType,
              typename Ret = BoundsRangeConvertible2<typename SourceType::Base,
                                                     typename TargetType::Base, Rank>>
    static auto helpBoundsRangeConvertible(SourceType, TargetType, std::true_type) -> Ret;

    template <std::size_t Rank, typename SourceType, typename TargetType>
    static auto helpBoundsRangeConvertible(SourceType, TargetType, ...) -> std::false_type;

    template <typename SourceType, typename TargetType, std::size_t Rank>
    struct BoundsRangeConvertible2
        : decltype(helpBoundsRangeConvertible<Rank - 1>(
              SourceType(), TargetType(),
              std::integral_constant<bool,
                                     SourceType::Depth == TargetType::Depth &&
                                         (SourceType::CurrentRange == TargetType::CurrentRange ||
                                          TargetType::CurrentRange == dynamic_range ||
                                          SourceType::CurrentRange == dynamic_range)>()))
    {
    };

    template <typename SourceType, typename TargetType>
    struct BoundsRangeConvertible2<SourceType, TargetType, 0> : std::true_type
    {
    };

    template <typename SourceType, typename TargetType, std::ptrdiff_t Rank = TargetType::Depth>
    struct BoundsRangeConvertible
        : decltype(helpBoundsRangeConvertible<Rank - 1>(
              SourceType(), TargetType(),
              std::integral_constant<bool,
                                     SourceType::Depth == TargetType::Depth &&
                                         (!details::LessThan<SourceType::CurrentRange,
                                                             TargetType::CurrentRange>::value ||
                                          TargetType::CurrentRange == dynamic_range ||
                                          SourceType::CurrentRange == dynamic_range)>()))
    {
    };

    template <typename SourceType, typename TargetType>
    struct BoundsRangeConvertible<SourceType, TargetType, 0> : std::true_type
    {
    };

    template <std::ptrdiff_t... Ranges,
              typename = std::enable_if_t<details::BoundsRangeConvertible<
                  details::BoundsRanges<Ranges...>,
                  details::BoundsRanges<FirstRange, RestRanges...>>::value>>
    constexpr static_bounds(const static_bounds<Ranges...>& other) : m_ranges(other.m_ranges)
    {
        Expects((MyRanges::DynamicNum == 0 && details::BoundsRanges<Ranges...>::DynamicNum == 0) ||
                MyRanges::DynamicNum > 0 || other.m_ranges.totalSize() >= m_ranges.totalSize());
    }

    constexpr static_bounds(std::initializer_list<size_type> il) : m_ranges(il.begin())
    {
        // Size of the initializer list must match the rank of the array
        Expects((MyRanges::DynamicNum == 0 && il.size() == 1 && *il.begin() == static_size) ||
                MyRanges::DynamicNum == il.size());
        // Size of the range must be less than the max element of the size type
        Expects(m_ranges.totalSize() <= PTRDIFF_MAX);
    }

    constexpr sliced_type slice() const noexcept
    {
        return sliced_type{static_cast<const details::BoundsRanges<RestRanges...>&>(m_ranges)};
    }

    constexpr size_type stride() const noexcept { return rank > 1 ? slice().size() : 1; }

    constexpr size_type size() const noexcept { return m_ranges.totalSize(); }

    constexpr size_type total_size() const noexcept { return m_ranges.totalSize(); }

    constexpr size_type linearize(const index_type& idx) const { return m_ranges.linearize(idx); }

    constexpr bool contains(const index_type& idx) const noexcept
    {
        return m_ranges.contains(idx) != -1;
    }

    constexpr size_type operator[](std::size_t idx) const noexcept
    {
        return m_ranges.elementNum(idx);
    }

    template <std::size_t Dim = 0>
    constexpr size_type extent() const noexcept
    {
        static_assert(Dim < rank,
                      "dimension should be less than rank (dimension count starts from 0)");
        return details::createTypeListIndexer(m_ranges).template get<Dim>().elementNum();
    }

    template <typename IntType>
    constexpr size_type extent(IntType dim) const
    {
        static_assert(std::is_integral<IntType>::value,
                      "Dimension parameter must be supplied as an integral type.");
        auto real_dim = narrow_cast<std::size_t>(dim);
        Expects(real_dim < rank);

        return m_ranges.elementNum(real_dim);
    }

    constexpr index_type index_bounds() const noexcept
    {
        size_type extents[rank] = {};
        m_ranges.serialize(extents);
        return {extents};
    }

    template <std::ptrdiff_t... Ranges>
    constexpr bool operator==(const static_bounds<Ranges...>& rhs) const noexcept
    {
        return this->size() == rhs.size();
    }

    template <std::ptrdiff_t... Ranges>
    constexpr bool operator!=(const static_bounds<Ranges...>& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    constexpr const_iterator begin() const noexcept
    {
        return const_iterator(*this, index_type{});
    }

    constexpr const_iterator end() const noexcept
    {
        return const_iterator(*this, this->index_bounds());
    }
};

template <std::size_t Rank>
class strided_bounds {
    template <std::size_t OtherRank>
    friend class strided_bounds;

public:
    static const std::size_t rank = Rank;
    using value_type = std::ptrdiff_t;
    using reference = std::add_lvalue_reference_t<value_type>;
    using const_reference = std::add_const_t<reference>;
    using size_type = value_type;
    using difference_type = value_type;
    using index_type = multi_span_index<rank>;
    using const_index_type = std::add_const_t<index_type>;
    using iterator = bounds_iterator<const_index_type>;
    using const_iterator = bounds_iterator<const_index_type>;
    static const value_type dynamic_rank = rank;
    static const value_type static_size = dynamic_range;
    using sliced_type = std::conditional_t<rank != 0, strided_bounds<rank - 1>, void>;
    using mapping_type = generalized_mapping_tag;

    constexpr strided_bounds(const strided_bounds&) noexcept = default;

    constexpr strided_bounds& operator=(const strided_bounds&) noexcept = default;

    constexpr strided_bounds(const value_type (&values)[rank], index_type strides)
        : m_extents(values), m_strides(std::move(strides))
    {}

    constexpr strided_bounds(const index_type& extents, const index_type& strides) noexcept
        : m_extents(extents), m_strides(strides)
    {
    }

    constexpr index_type strides() const noexcept { return m_strides; }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr size_type total_size() const noexcept
    {
        size_type ret = 0;
        for (std::size_t i = 0; i < rank; ++i) { ret += (m_extents[i] - 1) * m_strides[i]; }
        return ret + 1;
    }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr size_type size() const noexcept
    {
        size_type ret = 1;
        for (std::size_t i = 0; i < rank; ++i) { ret *= m_extents[i]; }
        return ret;
    }

    constexpr bool contains(const index_type& idx) const noexcept
    {
        for (std::size_t i = 0; i < rank; ++i)
        {
            if (idx[i] < 0 || idx[i] >= m_extents[i]) return false;
        }
        return true;
    }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr size_type linearize(const index_type& idx) const
    {
        size_type ret = 0;
        for (std::size_t i = 0; i < rank; i++)
        {
            Expects(idx[i] < m_extents[i]); // index is out of bounds of the array
            ret += idx[i] * m_strides[i];
        }
        return ret;
    }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr size_type stride() const noexcept { return m_strides[0]; }

    template <bool Enabled = (rank > 1), typename Ret = std::enable_if_t<Enabled, sliced_type>>
    constexpr sliced_type slice() const
    {
        return {details::shift_left(m_extents), details::shift_left(m_strides)};
    }

    template <std::size_t Dim = 0>

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr size_type extent() const noexcept
    {
        static_assert(Dim < Rank,
                      "dimension should be less than rank (dimension count starts from 0)");
        return m_extents[Dim];
    }

    constexpr index_type index_bounds() const noexcept { return m_extents; }

    constexpr const_iterator begin() const noexcept { return const_iterator{*this, index_type{}}; }

    constexpr const_iterator end() const noexcept { return const_iterator{*this, index_bounds()}; }

private:
    index_type m_extents;
    index_type m_strides;
};

template <typename T>
struct is_bounds : std::integral_constant<bool, false>
{
};
template <std::ptrdiff_t... Ranges>
struct is_bounds<static_bounds<Ranges...>> : std::integral_constant<bool, true>
{
};
template <std::size_t Rank>
struct is_bounds<strided_bounds<Rank>> : std::integral_constant<bool, true>
{
};

template <typename IndexType>
class bounds_iterator
{
public:
    static const std::size_t rank = IndexType::rank;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = IndexType;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using index_type = value_type;
    using index_size_type = typename IndexType::value_type;
    template <typename Bounds>
    explicit bounds_iterator(const Bounds& bnd, value_type curr) noexcept
        : boundary_(bnd.index_bounds()), curr_(std::move(curr))
    {
        static_assert(is_bounds<Bounds>::value, "Bounds type must be provided");
    }

    constexpr reference operator*() const noexcept { return curr_; }

    constexpr pointer operator->() const noexcept { return &curr_; }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    GSL_SUPPRESS(bounds.2) // NO-FORMAT: attribute
    constexpr bounds_iterator& operator++() noexcept

    {
        for (std::size_t i = rank; i-- > 0;)
        {
            if (curr_[i] < boundary_[i] - 1)
            {
                curr_[i]++;
                return *this;
            }
            curr_[i] = 0;
        }
        // If we're here we've wrapped over - set to past-the-end.
        curr_ = boundary_;
        return *this;
    }

    constexpr bounds_iterator operator++(int) noexcept
    {
        auto ret = *this;
        ++(*this);
        return ret;
    }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr bounds_iterator& operator--()
    {
        if (!less(curr_, boundary_))
        {
            // if at the past-the-end, set to last element
            for (std::size_t i = 0; i < rank; ++i) { curr_[i] = boundary_[i] - 1; }
            return *this;
        }
        for (std::size_t i = rank; i-- > 0;)
        {
            if (curr_[i] >= 1)
            {
                curr_[i]--;
                return *this;
            }
            curr_[i] = boundary_[i] - 1;
        }
        // If we're here the preconditions were violated
        // "pre: there exists s such that r == ++s"
        Expects(false);
        return *this;
    }

    constexpr bounds_iterator operator--(int) noexcept
    {
        auto ret = *this;
        --(*this);
        return ret;
    }

    constexpr bounds_iterator operator+(difference_type n) const noexcept
    {
        bounds_iterator ret{*this};
        return ret += n;
    }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr bounds_iterator& operator+=(difference_type n)
    {
        auto linear_idx = linearize(curr_) + n;
        std::remove_const_t<value_type> stride = 0;
        stride[rank - 1] = 1;
        for (std::size_t i = rank - 1; i-- > 0;) { stride[i] = stride[i + 1] * boundary_[i + 1]; }
        for (std::size_t i = 0; i < rank; ++i)
        {
            curr_[i] = linear_idx / stride[i];
            linear_idx = linear_idx % stride[i];
        }
        // index is out of bounds of the array
        Expects(!less(curr_, index_type{}) && !less(boundary_, curr_));
        return *this;
    }

    constexpr bounds_iterator operator-(difference_type n) const noexcept
    {
        bounds_iterator ret{*this};
        return ret -= n;
    }

    constexpr bounds_iterator& operator-=(difference_type n) noexcept { return *this += -n; }

    constexpr difference_type operator-(const bounds_iterator& rhs) const noexcept
    {
        return linearize(curr_) - linearize(rhs.curr_);
    }

    constexpr value_type operator[](difference_type n) const noexcept { return *(*this + n); }

    constexpr bool operator==(const bounds_iterator& rhs) const noexcept
    {
        return curr_ == rhs.curr_;
    }


    constexpr bool operator!=(const bounds_iterator& rhs) const noexcept { return !(*this == rhs); }

    constexpr bool operator<(const bounds_iterator& rhs) const noexcept
    {
        return less(curr_, rhs.curr_);
    }

    constexpr bool operator<=(const bounds_iterator& rhs) const noexcept { return !(rhs < *this); }

    constexpr bool operator>(const bounds_iterator& rhs) const noexcept { return rhs < *this; }

    constexpr bool operator>=(const bounds_iterator& rhs) const noexcept { return !(rhs > *this); }

    void swap(bounds_iterator& rhs) noexcept
    {
        std::swap(boundary_, rhs.boundary_);
        std::swap(curr_, rhs.curr_);
    }

private:

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr bool less(index_type& one, index_type& other) const noexcept
    {
        for (std::size_t i = 0; i < rank; ++i)
        {
            if (one[i] < other[i]) return true;
        }
        return false;
    }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    constexpr index_size_type linearize(const value_type& idx) const noexcept
    {
        // TODO: Smarter impl.
        // Check if past-the-end
        index_size_type multiplier = 1;
        index_size_type res = 0;
        if (!less(idx, boundary_))
        {
            res = 1;
            for (std::size_t i = rank; i-- > 0;)
            {
                res += (idx[i] - 1) * multiplier;
                multiplier *= boundary_[i];
            }
        }
        else
        {
            for (std::size_t i = rank; i-- > 0;)
            {
                res += idx[i] * multiplier;
                multiplier *= boundary_[i];
            }
        }
        return res;
    }

    value_type boundary_;
    std::remove_const_t<value_type> curr_;
};

template <typename IndexType>
bounds_iterator<IndexType> operator+(typename bounds_iterator<IndexType>::difference_type n,
                                     const bounds_iterator<IndexType>& rhs) noexcept
{
    return rhs + n;
}

namespace details
{
    template <typename Bounds>
    constexpr std::enable_if_t<
        std::is_same<typename Bounds::mapping_type, generalized_mapping_tag>::value,
        typename Bounds::index_type>
    make_stride(const Bounds& bnd) noexcept
    {
        return bnd.strides();
    }

    // Make a stride vector from bounds, assuming contiguous memory.
    template <typename Bounds>
    constexpr std::enable_if_t<
        std::is_same<typename Bounds::mapping_type, contiguous_mapping_tag>::value,
        typename Bounds::index_type>
    make_stride(const Bounds& bnd) noexcept
    {
        auto extents = bnd.index_bounds();
        typename Bounds::size_type stride[Bounds::rank] = {};

        stride[Bounds::rank - 1] = 1;
        for (std::size_t i = 1; i < Bounds::rank; ++i)
        {
            GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
            GSL_SUPPRESS(bounds.2) // NO-FORMAT: attribute
            stride[Bounds::rank - i - 1] = stride[Bounds::rank - i] * extents[Bounds::rank - i];
        }
        return {stride};
    }

    template <typename BoundsSrc, typename BoundsDest>
    void verifyBoundsReshape(const BoundsSrc& src, const BoundsDest& dest)
    {
        static_assert(is_bounds<BoundsSrc>::value && is_bounds<BoundsDest>::value,
                      "The src type and dest type must be bounds");
        static_assert(std::is_same<typename BoundsSrc::mapping_type, contiguous_mapping_tag>::value,
                      "The source type must be a contiguous bounds");
        static_assert(BoundsDest::static_size == dynamic_range ||
                          BoundsSrc::static_size == dynamic_range ||
                          BoundsDest::static_size == BoundsSrc::static_size,
                      "The source bounds must have same size as dest bounds");
        Expects(src.size() == dest.size());
    }

} // namespace details

template <typename Span>
class contiguous_span_iterator;
template <typename Span>
class general_span_iterator;

template <std::ptrdiff_t DimSize = dynamic_range>
struct dim_t
{
    static const std::ptrdiff_t value = DimSize;
};
template <>
struct dim_t<dynamic_range>
{
    static const std::ptrdiff_t value = dynamic_range;
    const std::ptrdiff_t dvalue;
    constexpr dim_t(std::ptrdiff_t size) noexcept : dvalue(size) {}
};

template <std::ptrdiff_t N, class = std::enable_if_t<(N >= 0)>>
constexpr dim_t<N> dim() noexcept
{
    return dim_t<N>();
}

template <std::ptrdiff_t N = dynamic_range, class = std::enable_if_t<N == dynamic_range>>
constexpr dim_t<N> dim(std::ptrdiff_t n) noexcept
{
    return dim_t<>(n);
}

template <typename ValueType, std::ptrdiff_t FirstDimension = dynamic_range,
          std::ptrdiff_t... RestDimensions>
class multi_span;
template <typename ValueType, std::size_t Rank>
class strided_span;

namespace details
{
    template <typename T, typename = std::true_type>
    struct SpanTypeTraits
    {
        using value_type = T;
        using size_type = std::size_t;
    };

    template <typename Traits>
    struct SpanTypeTraits<Traits, typename std::is_reference<typename Traits::span_traits&>::type>
    {
        using value_type = typename Traits::span_traits::value_type;
        using size_type = typename Traits::span_traits::size_type;
    };

    template <typename T, std::ptrdiff_t... Ranks>
    struct SpanArrayTraits
    {
        using type = multi_span<T, Ranks...>;
        using value_type = T;
        using bounds_type = static_bounds<Ranks...>;
        using pointer = T*;
        using reference = T&;
    };
    template <typename T, std::ptrdiff_t N, std::ptrdiff_t... Ranks>
    struct SpanArrayTraits<T[N], Ranks...> : SpanArrayTraits<T, Ranks..., N>
    {
    };

    template <typename BoundsType>
    BoundsType newBoundsHelperImpl(std::ptrdiff_t totalSize, std::true_type) // dynamic size
    {
        Expects(totalSize >= 0 && totalSize <= PTRDIFF_MAX);
        return BoundsType{totalSize};
    }
    template <typename BoundsType>
    BoundsType newBoundsHelperImpl(std::ptrdiff_t totalSize, std::false_type) // static size
    {
        Expects(BoundsType::static_size <= totalSize);
        return {};
    }
    template <typename BoundsType>
    BoundsType newBoundsHelper(std::ptrdiff_t totalSize)
    {
        static_assert(BoundsType::dynamic_rank <= 1, "dynamic rank must less or equal to 1");
        return newBoundsHelperImpl<BoundsType>(
            totalSize, std::integral_constant<bool, BoundsType::dynamic_rank == 1>());
    }

    struct Sep
    {
    };

    template <typename T, typename... Args>
    T static_as_multi_span_helper(Sep, Args... args)
    {
        return T{narrow_cast<typename T::size_type>(args)...};
    }
    template <typename T, typename Arg, typename... Args>
    std::enable_if_t<
        !std::is_same<Arg, dim_t<dynamic_range>>::value && !std::is_same<Arg, Sep>::value, T>
    static_as_multi_span_helper(Arg, Args... args)
    {
        return static_as_multi_span_helper<T>(args...);
    }
    template <typename T, typename... Args>
    T static_as_multi_span_helper(dim_t<dynamic_range> val, Args... args)
    {
        return static_as_multi_span_helper<T>(args..., val.dvalue);
    }

    template <typename... Dimensions>
    struct static_as_multi_span_static_bounds_helper
    {
        using type = static_bounds<(Dimensions::value)...>;
    };

    template <typename T>
    struct is_multi_span_oracle : std::false_type
    {
    };

    template <typename ValueType, std::ptrdiff_t FirstDimension, std::ptrdiff_t... RestDimensions>
    struct is_multi_span_oracle<multi_span<ValueType, FirstDimension, RestDimensions...>>
        : std::true_type
    {
    };

    template <typename ValueType, std::ptrdiff_t Rank>
    struct is_multi_span_oracle<strided_span<ValueType, Rank>> : std::true_type
    {
    };

    template <typename T>
    struct is_multi_span : is_multi_span_oracle<std::remove_cv_t<T>>
    {
    };
} // namespace details

template <typename ValueType, std::ptrdiff_t FirstDimension, std::ptrdiff_t... RestDimensions>
class multi_span
{
    // TODO do we still need this?
    template <typename ValueType2, std::ptrdiff_t FirstDimension2,
              std::ptrdiff_t... RestDimensions2>
    friend class multi_span;

public:
    using bounds_type = static_bounds<FirstDimension, RestDimensions...>;
    static const std::size_t Rank = bounds_type::rank;
    using size_type = typename bounds_type::size_type;
    using index_type = typename bounds_type::index_type;
    using value_type = ValueType;
    using const_value_type = std::add_const_t<value_type>;
    using pointer = std::add_pointer_t<value_type>;
    using reference = std::add_lvalue_reference_t<value_type>;
    using iterator = contiguous_span_iterator<multi_span>;
    using const_span = multi_span<const_value_type, FirstDimension, RestDimensions...>;
    using const_iterator = contiguous_span_iterator<const_span>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using sliced_type =
        std::conditional_t<Rank == 1, value_type, multi_span<value_type, RestDimensions...>>;

private:
    pointer data_;
    bounds_type bounds_;

    friend iterator;
    friend const_iterator;

public:
    // default constructor - same as constructing from nullptr_t
    GSL_SUPPRESS(type.6) // NO-FORMAT: attribute // TODO: false positive
    constexpr multi_span() noexcept
        : multi_span(nullptr, bounds_type{})
    {
        static_assert(bounds_type::dynamic_rank != 0 ||
                          (bounds_type::dynamic_rank == 0 && bounds_type::static_size == 0),
                      "Default construction of multi_span<T> only possible "
                      "for dynamic or fixed, zero-length spans.");
    }

    // construct from nullptr - get an empty multi_span
    GSL_SUPPRESS(type.6) // NO-FORMAT: attribute // TODO: false positive
    constexpr multi_span(std::nullptr_t) noexcept
        : multi_span(nullptr, bounds_type{})
    {
        static_assert(bounds_type::dynamic_rank != 0 ||
                          (bounds_type::dynamic_rank == 0 && bounds_type::static_size == 0),
                      "nullptr_t construction of multi_span<T> only possible "
                      "for dynamic or fixed, zero-length spans.");
    }

    // construct from nullptr with size of 0 (helps with template function calls)
    template <class IntType, typename = std::enable_if_t<std::is_integral<IntType>::value>>

    // GSL_SUPPRESS(type.6) // NO-FORMAT: attribute // TODO: false positive // TODO: parser bug
    constexpr multi_span(std::nullptr_t, IntType size) : multi_span(nullptr, bounds_type{})
    {
        static_assert(bounds_type::dynamic_rank != 0 ||
                          (bounds_type::dynamic_rank == 0 && bounds_type::static_size == 0),
                      "nullptr_t construction of multi_span<T> only possible "
                      "for dynamic or fixed, zero-length spans.");
        Expects(size == 0);
    }

    // construct from a single element

    GSL_SUPPRESS(type.6) // NO-FORMAT: attribute // TODO: false positive
    constexpr multi_span(reference data) noexcept
        : multi_span(&data, bounds_type{1})
    {
        static_assert(bounds_type::dynamic_rank > 0 || bounds_type::static_size == 0 ||
                          bounds_type::static_size == 1,
                      "Construction from a single element only possible "
                      "for dynamic or fixed spans of length 0 or 1.");
    }

    // prevent constructing from temporaries for single-elements
    constexpr multi_span(value_type&&) = delete;

    // construct from pointer + length
    GSL_SUPPRESS(type.6) // NO-FORMAT: attribute // TODO: false positive
    constexpr multi_span(pointer ptr, size_type size)
        : multi_span(ptr, bounds_type{size})
    {}

    // construct from pointer + length - multidimensional
    constexpr multi_span(pointer data, bounds_type bounds)
        : data_(data), bounds_(std::move(bounds))
    {
        Expects((bounds_.size() > 0 && data != nullptr) || bounds_.size() == 0);
    }

    // construct from begin,end pointer pair
    template <typename Ptr,
              typename = std::enable_if_t<std::is_convertible<Ptr, pointer>::value &&
                                          details::LessThan<bounds_type::dynamic_rank, 2>::value>>
    constexpr multi_span(pointer begin, Ptr end)
        : multi_span(begin,
                     details::newBoundsHelper<bounds_type>(static_cast<pointer>(end) - begin))
    {
        Expects(begin != nullptr && end != nullptr && begin <= static_cast<pointer>(end));
    }

    // construct from n-dimensions static array
    template <typename T, std::size_t N, typename Helper = details::SpanArrayTraits<T, N>>
    constexpr multi_span(T (&arr)[N])
        : multi_span(reinterpret_cast<pointer>(arr), bounds_type{typename Helper::bounds_type{}})
    {
        static_assert(std::is_convertible<typename Helper::value_type(*)[], value_type(*)[]>::value,
                      "Cannot convert from source type to target multi_span type.");
        static_assert(std::is_convertible<typename Helper::bounds_type, bounds_type>::value,
                      "Cannot construct a multi_span from an array with fewer elements.");
    }

    // construct from n-dimensions dynamic array (e.g. new int[m][4])
    // (precedence will be lower than the 1-dimension pointer)
    template <typename T, typename Helper = details::SpanArrayTraits<T, dynamic_range>>
    constexpr multi_span(T* const& data, size_type size)
        : multi_span(reinterpret_cast<pointer>(data), typename Helper::bounds_type{size})
    {
        static_assert(std::is_convertible<typename Helper::value_type(*)[], value_type(*)[]>::value,
                      "Cannot convert from source type to target multi_span type.");
    }

    // construct from std::array
    template <typename T, std::size_t N>
    constexpr multi_span(std::array<T, N>& arr)
        : multi_span(arr.data(), bounds_type{static_bounds<N>{}})
    {
        static_assert(
            std::is_convertible<T(*)[], typename std::remove_const_t<value_type>(*)[]>::value,
            "Cannot convert from source type to target multi_span type.");
        static_assert(std::is_convertible<static_bounds<N>, bounds_type>::value,
                      "You cannot construct a multi_span from a std::array of smaller size.");
    }

    // construct from const std::array
    template <typename T, std::size_t N>
    constexpr multi_span(const std::array<T, N>& arr)
        : multi_span(arr.data(), bounds_type{static_bounds<N>{}})
    {
        static_assert(
            std::is_convertible<T(*)[], typename std::remove_const_t<value_type>(*)[]>::value,
            "Cannot convert from source type to target multi_span type.");
        static_assert(std::is_convertible<static_bounds<N>, bounds_type>::value,
                      "You cannot construct a multi_span from a std::array of smaller size.");
    }

    // prevent constructing from temporary std::array
    template <typename T, std::size_t N>
    constexpr multi_span(std::array<T, N>&& arr) = delete;

    // construct from containers
    // future: could use contiguous_iterator_traits to identify only contiguous containers
    // type-requirements: container must have .size(), operator[] which are value_type compatible
    template <typename Cont, typename DataType = typename Cont::value_type,
              typename = std::enable_if_t<
                  !details::is_multi_span<Cont>::value &&
                  std::is_convertible<DataType (*)[], value_type (*)[]>::value &&
                  std::is_same<std::decay_t<decltype(std::declval<Cont>().size(),
                                                     *std::declval<Cont>().data())>,
                               DataType>::value>>
    constexpr multi_span(Cont& cont)
        : multi_span(static_cast<pointer>(cont.data()),
                     details::newBoundsHelper<bounds_type>(narrow_cast<size_type>(cont.size())))
    {}

    // prevent constructing from temporary containers
    template <typename Cont, typename DataType = typename Cont::value_type,
              typename = std::enable_if_t<
                  !details::is_multi_span<Cont>::value &&
                  std::is_convertible<DataType (*)[], value_type (*)[]>::value &&
                  std::is_same<std::decay_t<decltype(std::declval<Cont>().size(),
                                                     *std::declval<Cont>().data())>,
                               DataType>::value>>
    explicit constexpr multi_span(Cont&& cont) = delete;

    // construct from a convertible multi_span
    template <typename OtherValueType, std::ptrdiff_t... OtherDimensions,
              typename OtherBounds = static_bounds<OtherDimensions...>,
              typename = std::enable_if_t<std::is_convertible<OtherValueType, ValueType>::value &&
                                          std::is_convertible<OtherBounds, bounds_type>::value>>
    constexpr multi_span(multi_span<OtherValueType, OtherDimensions...> other)
        : data_(other.data_), bounds_(other.bounds_)
    {}

    // trivial copy and move
    constexpr multi_span(const multi_span&) = default;
    constexpr multi_span(multi_span&&) = default;

    // trivial assignment
    constexpr multi_span& operator=(const multi_span&) = default;
    constexpr multi_span& operator=(multi_span&&) = default;

    // first() - extract the first Count elements into a new multi_span
    template <std::ptrdiff_t Count>

    constexpr multi_span<ValueType, Count> first() const
    {
        static_assert(Count >= 0, "Count must be >= 0.");
        static_assert(bounds_type::static_size == dynamic_range ||
                          Count <= bounds_type::static_size,
                      "Count is out of bounds.");

        Expects(bounds_type::static_size != dynamic_range || Count <= this->size());
        return {this->data(), Count};
    }

    // first() - extract the first count elements into a new multi_span
    constexpr multi_span<ValueType, dynamic_range> first(size_type count) const
    {
        Expects(count >= 0 && count <= this->size());
        return {this->data(), count};
    }

    // last() - extract the last Count elements into a new multi_span
    template <std::ptrdiff_t Count>
    constexpr multi_span<ValueType, Count> last() const
    {
        static_assert(Count >= 0, "Count must be >= 0.");
        static_assert(bounds_type::static_size == dynamic_range ||
                          Count <= bounds_type::static_size,
                      "Count is out of bounds.");

        Expects(bounds_type::static_size != dynamic_range || Count <= this->size());
        return {this->data() + this->size() - Count, Count};
    }

    // last() - extract the last count elements into a new multi_span
    constexpr multi_span<ValueType, dynamic_range> last(size_type count) const
    {
        Expects(count >= 0 && count <= this->size());
        return {this->data() + this->size() - count, count};
    }

    // subspan() - create a subview of Count elements starting at Offset
    template <std::ptrdiff_t Offset, std::ptrdiff_t Count>
    constexpr multi_span<ValueType, Count> subspan() const
    {
        static_assert(Count >= 0, "Count must be >= 0.");
        static_assert(Offset >= 0, "Offset must be >= 0.");
        static_assert(bounds_type::static_size == dynamic_range ||
                          ((Offset <= bounds_type::static_size) &&
                           Count <= bounds_type::static_size - Offset),
                      "You must describe a sub-range within bounds of the multi_span.");

        Expects(bounds_type::static_size != dynamic_range ||
                (Offset <= this->size() && Count <= this->size() - Offset));
        return {this->data() + Offset, Count};
    }

    // subspan() - create a subview of count elements starting at offset
    // supplying dynamic_range for count will consume all available elements from offset
    constexpr multi_span<ValueType, dynamic_range> subspan(size_type offset,
                                                           size_type count = dynamic_range) const
    {
        Expects((offset >= 0 && offset <= this->size()) &&
                (count == dynamic_range || (count <= this->size() - offset)));
        return {this->data() + offset, count == dynamic_range ? this->length() - offset : count};
    }

    // section - creates a non-contiguous, strided multi_span from a contiguous one
    constexpr strided_span<ValueType, Rank> section(index_type origin,
                                                    index_type extents) const
    {
        const size_type size = this->bounds().total_size() - this->bounds().linearize(origin);
        return {&this->operator[](origin), size,
                strided_bounds<Rank>{extents, details::make_stride(bounds())}};
    }

    // length of the multi_span in elements
    constexpr size_type size() const noexcept { return bounds_.size(); }

    // length of the multi_span in elements
    constexpr size_type length() const noexcept { return this->size(); }

    // length of the multi_span in bytes
    constexpr size_type size_bytes() const noexcept
    {
        return narrow_cast<size_type>(sizeof(value_type)) * this->size();
    }

    // length of the multi_span in bytes
    constexpr size_type length_bytes() const noexcept { return this->size_bytes(); }

    constexpr bool empty() const noexcept { return this->size() == 0; }

    static constexpr std::size_t rank() { return Rank; }

    template <std::size_t Dim = 0>
    constexpr size_type extent() const noexcept
    {
        static_assert(Dim < Rank,
                      "Dimension should be less than rank (dimension count starts from 0).");
        return bounds_.template extent<Dim>();
    }

    template <typename IntType>
    constexpr size_type extent(IntType dim) const
    {
        return bounds_.extent(dim);
    }

    constexpr bounds_type bounds() const noexcept { return bounds_; }

    constexpr pointer data() const noexcept { return data_; }

    template <typename FirstIndex>
    constexpr reference operator()(FirstIndex idx)
    {
        return this->operator[](narrow_cast<std::ptrdiff_t>(idx));
    }

    template <typename FirstIndex, typename... OtherIndices>
    constexpr reference operator()(FirstIndex firstIndex, OtherIndices... indices)
    {
        const index_type idx = {narrow_cast<std::ptrdiff_t>(firstIndex),
                                narrow_cast<std::ptrdiff_t>(indices)...};
        return this->operator[](idx);
    }

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    constexpr reference operator[](const index_type& idx) const
    {
        return data_[bounds_.linearize(idx)];
    }

    template <bool Enabled = (Rank > 1), typename Ret = std::enable_if_t<Enabled, sliced_type>>

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    constexpr Ret operator[](size_type idx) const
    {
        Expects(idx >= 0 && idx < bounds_.size()); // index is out of bounds of the array
        const size_type ridx = idx * bounds_.stride();

        // index is out of bounds of the underlying data
        Expects(ridx < bounds_.total_size());
        return Ret{data_ + ridx, bounds_.slice()};
    }

    constexpr iterator begin() const noexcept { return iterator{this, true}; }

    constexpr iterator end() const noexcept { return iterator{this, false}; }

    GSL_SUPPRESS(type.1) // NO-FORMAT: attribute
    constexpr const_iterator cbegin() const noexcept
    {
        return const_iterator{reinterpret_cast<const const_span*>(this), true};
    }

    constexpr const_iterator cend() const noexcept
    {
        return const_iterator{reinterpret_cast<const const_span*>(this), false};
    }

    constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator{end()}; }

    constexpr reverse_iterator rend() const noexcept { return reverse_iterator{begin()}; }

    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator{cend()};
    }

    constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator{cbegin()};
    }

    template <typename OtherValueType, std::ptrdiff_t... OtherDimensions,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator==(const multi_span<OtherValueType, OtherDimensions...>& other) const
    {
        return bounds_.size() == other.bounds_.size() &&
               (data_ == other.data_ || std::equal(this->begin(), this->end(), other.begin()));
    }

    template <typename OtherValueType, std::ptrdiff_t... OtherDimensions,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator!=(const multi_span<OtherValueType, OtherDimensions...>& other) const
    {
        return !(*this == other);
    }

    template <typename OtherValueType, std::ptrdiff_t... OtherDimensions,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator<(const multi_span<OtherValueType, OtherDimensions...>& other) const
    {
        return std::lexicographical_compare(this->begin(), this->end(), other.begin(), other.end());
    }

    template <typename OtherValueType, std::ptrdiff_t... OtherDimensions,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator<=(const multi_span<OtherValueType, OtherDimensions...>& other) const
    {
        return !(other < *this);
    }

    template <typename OtherValueType, std::ptrdiff_t... OtherDimensions,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator>(const multi_span<OtherValueType, OtherDimensions...>& other) const
        noexcept
    {
        return (other < *this);
    }

    template <typename OtherValueType, std::ptrdiff_t... OtherDimensions,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator>=(const multi_span<OtherValueType, OtherDimensions...>& other) const
    {
        return !(*this < other);
    }
};

//
// Free functions for manipulating spans
//

// reshape a multi_span into a different dimensionality
// DimCount and Enabled here are workarounds for a bug in MSVC 2015
template <typename SpanType, typename... Dimensions2, std::size_t DimCount = sizeof...(Dimensions2),
          bool Enabled = (DimCount > 0), typename = std::enable_if_t<Enabled>>
constexpr auto as_multi_span(SpanType s, Dimensions2... dims)
    -> multi_span<typename SpanType::value_type, Dimensions2::value...>
{
    static_assert(details::is_multi_span<SpanType>::value,
                  "Variadic as_multi_span() is for reshaping existing spans.");
    using BoundsType =
        typename multi_span<typename SpanType::value_type, (Dimensions2::value)...>::bounds_type;
    const auto tobounds = details::static_as_multi_span_helper<BoundsType>(dims..., details::Sep{});
    details::verifyBoundsReshape(s.bounds(), tobounds);
    return {s.data(), tobounds};
}

// convert a multi_span<T> to a multi_span<const byte>
template <typename U, std::ptrdiff_t... Dimensions>
multi_span<const byte, dynamic_range> as_bytes(multi_span<U, Dimensions...> s) noexcept
{
    static_assert(std::is_trivial<std::decay_t<U>>::value,
                  "The value_type of multi_span must be a trivial type.");
    return {reinterpret_cast<const byte*>(s.data()), s.size_bytes()};
}

// convert a multi_span<T> to a multi_span<byte> (a writeable byte multi_span)
// this is not currently a portable function that can be relied upon to work
// on all implementations. It should be considered an experimental extension
// to the standard GSL interface.
template <typename U, std::ptrdiff_t... Dimensions>
multi_span<byte> as_writeable_bytes(multi_span<U, Dimensions...> s) noexcept
{
    static_assert(std::is_trivial<std::decay_t<U>>::value,
                  "The value_type of multi_span must be a trivial type.");
    return {reinterpret_cast<byte*>(s.data()), s.size_bytes()};
}

// convert a multi_span<const byte> to a multi_span<const T>
// this is not currently a portable function that can be relied upon to work
// on all implementations. It should be considered an experimental extension
// to the standard GSL interface.
template <typename U, std::ptrdiff_t... Dimensions>
constexpr auto as_multi_span(multi_span<const byte, Dimensions...> s) -> multi_span<
    const U, static_cast<std::ptrdiff_t>(
                 multi_span<const byte, Dimensions...>::bounds_type::static_size != dynamic_range
                     ? (static_cast<std::size_t>(
                            multi_span<const byte, Dimensions...>::bounds_type::static_size) /
                        sizeof(U))
                     : dynamic_range)>
{
    using ConstByteSpan = multi_span<const byte, Dimensions...>;
    static_assert(
        std::is_trivial<std::decay_t<U>>::value &&
            (ConstByteSpan::bounds_type::static_size == dynamic_range ||
             ConstByteSpan::bounds_type::static_size % narrow_cast<std::ptrdiff_t>(sizeof(U)) == 0),
        "Target type must be a trivial type and its size must match the byte array size");

    Expects((s.size_bytes() % narrow_cast<std::ptrdiff_t>(sizeof(U))) == 0 &&
            (s.size_bytes() / narrow_cast<std::ptrdiff_t>(sizeof(U))) < PTRDIFF_MAX);
    return {reinterpret_cast<const U*>(s.data()),
            s.size_bytes() / narrow_cast<std::ptrdiff_t>(sizeof(U))};
}

// convert a multi_span<byte> to a multi_span<T>
// this is not currently a portable function that can be relied upon to work
// on all implementations. It should be considered an experimental extension
// to the standard GSL interface.
template <typename U, std::ptrdiff_t... Dimensions>
constexpr auto as_multi_span(multi_span<byte, Dimensions...> s)
    -> multi_span<U, narrow_cast<std::ptrdiff_t>(
                         multi_span<byte, Dimensions...>::bounds_type::static_size != dynamic_range
                             ? static_cast<std::size_t>(
                                   multi_span<byte, Dimensions...>::bounds_type::static_size) /
                                   sizeof(U)
                             : dynamic_range)>
{
    using ByteSpan = multi_span<byte, Dimensions...>;
    static_assert(std::is_trivial<std::decay_t<U>>::value &&
                      (ByteSpan::bounds_type::static_size == dynamic_range ||
                       ByteSpan::bounds_type::static_size % sizeof(U) == 0),
                  "Target type must be a trivial type and its size must match the byte array size");

    Expects((s.size_bytes() % sizeof(U)) == 0);
    return {reinterpret_cast<U*>(s.data()),
            s.size_bytes() / narrow_cast<std::ptrdiff_t>(sizeof(U))};
}

template <typename T, std::ptrdiff_t... Dimensions>
constexpr auto as_multi_span(T* const& ptr, dim_t<Dimensions>... args)
    -> multi_span<std::remove_all_extents_t<T>, Dimensions...>
{
    return {reinterpret_cast<std::remove_all_extents_t<T>*>(ptr),
            details::static_as_multi_span_helper<static_bounds<Dimensions...>>(args...,
                                                                               details::Sep{})};
}

template <typename T>
constexpr auto as_multi_span(T* arr, std::ptrdiff_t len) ->
    typename details::SpanArrayTraits<T, dynamic_range>::type
{
    return {reinterpret_cast<std::remove_all_extents_t<T>*>(arr), len};
}

template <typename T, std::size_t N>
constexpr auto as_multi_span(T (&arr)[N]) -> typename details::SpanArrayTraits<T, N>::type
{
    return {arr};
}

template <typename T, std::size_t N>
constexpr multi_span<const T, N> as_multi_span(const std::array<T, N>& arr)
{
    return {arr};
}

template <typename T, std::size_t N>
constexpr multi_span<const T, N> as_multi_span(const std::array<T, N>&&) = delete;

template <typename T, std::size_t N>
constexpr multi_span<T, N> as_multi_span(std::array<T, N>& arr)
{
    return {arr};
}

template <typename T>
constexpr multi_span<T, dynamic_range> as_multi_span(T* begin, T* end)
{
    return {begin, end};
}

template <typename Cont>
constexpr auto as_multi_span(Cont& arr) -> std::enable_if_t<
    !details::is_multi_span<std::decay_t<Cont>>::value,
    multi_span<std::remove_reference_t<decltype(arr.size(), *arr.data())>, dynamic_range>>
{
    Expects(arr.size() < PTRDIFF_MAX);
    return {arr.data(), narrow_cast<std::ptrdiff_t>(arr.size())};
}

template <typename Cont>
constexpr auto as_multi_span(Cont&& arr) -> std::enable_if_t<
    !details::is_multi_span<std::decay_t<Cont>>::value,
    multi_span<std::remove_reference_t<decltype(arr.size(), *arr.data())>, dynamic_range>> = delete;

// from basic_string which doesn't have nonconst .data() member like other contiguous containers
template <typename CharT, typename Traits, typename Allocator>
GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
constexpr auto as_multi_span(std::basic_string<CharT, Traits, Allocator>& str)
    -> multi_span<CharT, dynamic_range>
{
    Expects(str.size() < PTRDIFF_MAX);
    return {&str[0], narrow_cast<std::ptrdiff_t>(str.size())};
}

// strided_span is an extension that is not strictly part of the GSL at this time.
// It is kept here while the multidimensional interface is still being defined.
template <typename ValueType, std::size_t Rank>
class strided_span
{
public:
    using bounds_type = strided_bounds<Rank>;
    using size_type = typename bounds_type::size_type;
    using index_type = typename bounds_type::index_type;
    using value_type = ValueType;
    using const_value_type = std::add_const_t<value_type>;
    using pointer = std::add_pointer_t<value_type>;
    using reference = std::add_lvalue_reference_t<value_type>;
    using iterator = general_span_iterator<strided_span>;
    using const_strided_span = strided_span<const_value_type, Rank>;
    using const_iterator = general_span_iterator<const_strided_span>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using sliced_type =
        std::conditional_t<Rank == 1, value_type, strided_span<value_type, Rank - 1>>;

private:
    pointer data_;
    bounds_type bounds_;

    friend iterator;
    friend const_iterator;
    template <typename OtherValueType, std::size_t OtherRank>
    friend class strided_span;

public:
    // from raw data
    constexpr strided_span(pointer ptr, size_type size, bounds_type bounds)
        : data_(ptr), bounds_(std::move(bounds))
    {
        Expects((bounds_.size() > 0 && ptr != nullptr) || bounds_.size() == 0);
        // Bounds cross data boundaries
        Expects(this->bounds().total_size() <= size);
        GSL_SUPPRESS(type.4) // NO-FORMAT: attribute // TODO: false positive
        (void) size;
    }

    // from static array of size N
    template <size_type N>
    constexpr strided_span(value_type (&values)[N], bounds_type bounds)
        : strided_span(values, N, std::move(bounds))
    {}

    // from array view
    template <typename OtherValueType, std::ptrdiff_t... Dimensions,
              bool Enabled1 = (sizeof...(Dimensions) == Rank),
              bool Enabled2 = std::is_convertible<OtherValueType*, ValueType*>::value,
              typename = std::enable_if_t<Enabled1 && Enabled2>>
    constexpr strided_span(multi_span<OtherValueType, Dimensions...> av, bounds_type bounds)
        : strided_span(av.data(), av.bounds().total_size(), std::move(bounds))
    {}

    // convertible
    template <typename OtherValueType, typename = std::enable_if_t<std::is_convertible<
                                           OtherValueType (*)[], value_type (*)[]>::value>>
    constexpr strided_span(const strided_span<OtherValueType, Rank>& other)
        : data_(other.data_), bounds_(other.bounds_)
    {}

    // convert from bytes
    template <typename OtherValueType>
    constexpr strided_span<
        typename std::enable_if<std::is_same<value_type, const byte>::value, OtherValueType>::type,
        Rank>
    as_strided_span() const
    {
        static_assert((sizeof(OtherValueType) >= sizeof(value_type)) &&
                          (sizeof(OtherValueType) % sizeof(value_type) == 0),
                      "OtherValueType should have a size to contain a multiple of ValueTypes");
        auto d = narrow_cast<size_type>(sizeof(OtherValueType) / sizeof(value_type));

        const size_type size = this->bounds().total_size() / d;
        return {const_cast<OtherValueType*>(reinterpret_cast<const OtherValueType*>(this->data())),
                size,
                bounds_type{resize_extent(this->bounds().index_bounds(), d),
                            resize_stride(this->bounds().strides(), d)}};
    }

    constexpr strided_span section(index_type origin, index_type extents) const
    {
        const size_type size = this->bounds().total_size() - this->bounds().linearize(origin);
        return {&this->operator[](origin), size,
                bounds_type{extents, details::make_stride(bounds())}};
    }

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    constexpr reference operator[](const index_type& idx) const
    {
        return data_[bounds_.linearize(idx)];
    }

    template <bool Enabled = (Rank > 1), typename Ret = std::enable_if_t<Enabled, sliced_type>>
    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    constexpr Ret operator[](size_type idx) const
    {
        Expects(idx < bounds_.size()); // index is out of bounds of the array
        const size_type ridx = idx * bounds_.stride();

        // index is out of bounds of the underlying data
        Expects(ridx < bounds_.total_size());
        return {data_ + ridx, bounds_.slice().total_size(), bounds_.slice()};
    }

    constexpr bounds_type bounds() const noexcept { return bounds_; }

    template <std::size_t Dim = 0>
    constexpr size_type extent() const noexcept
    {
        static_assert(Dim < Rank,
                      "dimension should be less than Rank (dimension count starts from 0)");
        return bounds_.template extent<Dim>();
    }

    constexpr size_type size() const noexcept { return bounds_.size(); }

    constexpr pointer data() const noexcept { return data_; }

    constexpr bool empty() const noexcept { return this->size() == 0; }

    constexpr explicit operator bool() const noexcept { return data_ != nullptr; }

    constexpr iterator begin() const { return iterator{this, true}; }

    constexpr iterator end() const { return iterator{this, false}; }

    constexpr const_iterator cbegin() const
    {
        return const_iterator{reinterpret_cast<const const_strided_span*>(this), true};
    }

    constexpr const_iterator cend() const
    {
        return const_iterator{reinterpret_cast<const const_strided_span*>(this), false};
    }

    constexpr reverse_iterator rbegin() const { return reverse_iterator{end()}; }

    constexpr reverse_iterator rend() const { return reverse_iterator{begin()}; }

    constexpr const_reverse_iterator crbegin() const { return const_reverse_iterator{cend()}; }

    constexpr const_reverse_iterator crend() const { return const_reverse_iterator{cbegin()}; }

    template <typename OtherValueType, std::ptrdiff_t OtherRank,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator==(const strided_span<OtherValueType, OtherRank>& other) const
    {
        return bounds_.size() == other.bounds_.size() &&
               (data_ == other.data_ || std::equal(this->begin(), this->end(), other.begin()));
    }

    template <typename OtherValueType, std::ptrdiff_t OtherRank,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator!=(const strided_span<OtherValueType, OtherRank>& other) const
    {
        return !(*this == other);
    }

    template <typename OtherValueType, std::ptrdiff_t OtherRank,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator<(const strided_span<OtherValueType, OtherRank>& other) const
    {
        return std::lexicographical_compare(this->begin(), this->end(), other.begin(), other.end());
    }

    template <typename OtherValueType, std::ptrdiff_t OtherRank,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator<=(const strided_span<OtherValueType, OtherRank>& other) const
    {
        return !(other < *this);
    }

    template <typename OtherValueType, std::ptrdiff_t OtherRank,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator>(const strided_span<OtherValueType, OtherRank>& other) const
    {
        return (other < *this);
    }

    template <typename OtherValueType, std::ptrdiff_t OtherRank,
              typename = std::enable_if_t<std::is_same<std::remove_cv_t<value_type>,
                                                       std::remove_cv_t<OtherValueType>>::value>>
    constexpr bool operator>=(const strided_span<OtherValueType, OtherRank>& other) const
    {
        return !(*this < other);
    }

private:
    static index_type resize_extent(const index_type& extent, std::ptrdiff_t d)
    {
        // The last dimension of the array needs to contain a multiple of new type elements
        GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
        Expects(extent[Rank - 1] >= d && (extent[Rank - 1] % d == 0));

        index_type ret = extent;
        ret[Rank - 1] /= d;

        return ret;
    }

    template <bool Enabled = (Rank == 1), typename = std::enable_if_t<Enabled>>
    static index_type resize_stride(const index_type& strides, std::ptrdiff_t, void* = nullptr)
    {
        // Only strided arrays with regular strides can be resized
        GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
        Expects(strides[Rank - 1] == 1);

        return strides;
    }

    template <bool Enabled = (Rank > 1), typename = std::enable_if_t<Enabled>>
    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    static index_type resize_stride(const index_type& strides, std::ptrdiff_t d)
    {
        // Only strided arrays with regular strides can be resized
        Expects(strides[Rank - 1] == 1);
        // The strides must have contiguous chunks of
        // memory that can contain a multiple of new type elements
        Expects(strides[Rank - 2] >= d && (strides[Rank - 2] % d == 0));

        for (std::size_t i = Rank - 1; i > 0; --i)
        {
            // Only strided arrays with regular strides can be resized
            Expects((strides[i - 1] >= strides[i]) && (strides[i - 1] % strides[i] == 0));
        }

        index_type ret = strides / d;
        ret[Rank - 1] = 1;

        return ret;
    }
};

template <class Span>
class contiguous_span_iterator
{
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = typename Span::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

private:
    template <typename ValueType, std::ptrdiff_t FirstDimension, std::ptrdiff_t... RestDimensions>
    friend class multi_span;

    pointer data_;
    const Span* m_validator;

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    void validateThis() const {
        // iterator is out of range of the array
        Expects(data_ >= m_validator->data_ && data_ < m_validator->data_ + m_validator->size());
    }

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    contiguous_span_iterator(const Span* container, bool isbegin)
        : data_(isbegin ? container->data_ : container->data_ + container->size())
        , m_validator(container)
    {}

public:
    reference operator*() const
    {
        validateThis();
        return *data_;
    }
    pointer operator->() const
    {
        validateThis();
        return data_;
    }

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    contiguous_span_iterator& operator++() noexcept
    {
        ++data_;
        return *this;
    }
    contiguous_span_iterator operator++(int) noexcept
    {
        auto ret = *this;
        ++(*this);
        return ret;
    }

    GSL_SUPPRESS(bounds.1) // NO-FORMAT: attribute
    contiguous_span_iterator& operator--() noexcept
    {
        --data_;
        return *this;
    }
    contiguous_span_iterator operator--(int) noexcept
    {
        auto ret = *this;
        --(*this);
        return ret;
    }
    contiguous_span_iterator operator+(difference_type n) const noexcept
    {
        contiguous_span_iterator ret{*this};
        return ret += n;
    }
    contiguous_span_iterator& operator+=(difference_type n) noexcept
    {
        data_ += n;
        return *this;
    }
    contiguous_span_iterator operator-(difference_type n) const noexcept
    {
        contiguous_span_iterator ret{*this};
        return ret -= n;
    }

    contiguous_span_iterator& operator-=(difference_type n) { return *this += -n; }
    difference_type operator-(const contiguous_span_iterator& rhs) const
    {
        Expects(m_validator == rhs.m_validator);
        return data_ - rhs.data_;
    }
    reference operator[](difference_type n) const { return *(*this + n); }
    bool operator==(const contiguous_span_iterator& rhs) const
    {
        Expects(m_validator == rhs.m_validator);
        return data_ == rhs.data_;
    }

    bool operator!=(const contiguous_span_iterator& rhs) const { return !(*this == rhs); }

    bool operator<(const contiguous_span_iterator& rhs) const
    {
        Expects(m_validator == rhs.m_validator);
        return data_ < rhs.data_;
    }

    bool operator<=(const contiguous_span_iterator& rhs) const { return !(rhs < *this); }
    bool operator>(const contiguous_span_iterator& rhs) const { return rhs < *this; }
    bool operator>=(const contiguous_span_iterator& rhs) const { return !(rhs > *this); }

    void swap(contiguous_span_iterator& rhs) noexcept
    {
        std::swap(data_, rhs.data_);
        std::swap(m_validator, rhs.m_validator);
    }
};

template <typename Span>
contiguous_span_iterator<Span> operator+(typename contiguous_span_iterator<Span>::difference_type n,
                                         const contiguous_span_iterator<Span>& rhs) noexcept
{
    return rhs + n;
}

template <typename Span>
class general_span_iterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = typename Span::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

private:
    template <typename ValueType, std::size_t Rank>
    friend class strided_span;

    const Span* m_container;
    typename Span::bounds_type::iterator m_itr;
    general_span_iterator(const Span* container, bool isbegin)
        : m_container(container)
        , m_itr(isbegin ? m_container->bounds().begin() : m_container->bounds().end())
    {}

public:
    reference operator*() noexcept { return (*m_container)[*m_itr]; }
    pointer operator->() noexcept { return &(*m_container)[*m_itr]; }
    general_span_iterator& operator++() noexcept
    {
        ++m_itr;
        return *this;
    }
    general_span_iterator operator++(int) noexcept
    {
        auto ret = *this;
        ++(*this);
        return ret;
    }
    general_span_iterator& operator--() noexcept
    {
        --m_itr;
        return *this;
    }
    general_span_iterator operator--(int) noexcept
    {
        auto ret = *this;
        --(*this);
        return ret;
    }
    general_span_iterator operator+(difference_type n) const noexcept
    {
        general_span_iterator ret{*this};
        return ret += n;
    }
    general_span_iterator& operator+=(difference_type n) noexcept
    {
        m_itr += n;
        return *this;
    }
    general_span_iterator operator-(difference_type n) const noexcept
    {
        general_span_iterator ret{*this};
        return ret -= n;
    }
    general_span_iterator& operator-=(difference_type n) noexcept { return *this += -n; }
    difference_type operator-(const general_span_iterator& rhs) const
    {
        Expects(m_container == rhs.m_container);
        return m_itr - rhs.m_itr;
    }

    GSL_SUPPRESS(bounds.4) // NO-FORMAT: attribute
    value_type operator[](difference_type n) const { return (*m_container)[m_itr[n]]; }

    bool operator==(const general_span_iterator& rhs) const
    {
        Expects(m_container == rhs.m_container);
        return m_itr == rhs.m_itr;
    }
    bool operator!=(const general_span_iterator& rhs) const { return !(*this == rhs); }
    bool operator<(const general_span_iterator& rhs) const
    {
        Expects(m_container == rhs.m_container);
        return m_itr < rhs.m_itr;
    }
    bool operator<=(const general_span_iterator& rhs) const { return !(rhs < *this); }
    bool operator>(const general_span_iterator& rhs) const { return rhs < *this; }
    bool operator>=(const general_span_iterator& rhs) const { return !(rhs > *this); }
    void swap(general_span_iterator& rhs) noexcept
    {
        std::swap(m_itr, rhs.m_itr);
        std::swap(m_container, rhs.m_container);
    }
};

template <typename Span>
general_span_iterator<Span> operator+(typename general_span_iterator<Span>::difference_type n,
                                      const general_span_iterator<Span>& rhs) noexcept
{
    return rhs + n;
}

} // namespace gsl

#ifdef _MSC_VER
#if _MSC_VER < 1910

#undef constexpr
#pragma pop_macro("constexpr")
#endif // _MSC_VER < 1910

#pragma warning(pop)

#endif // _MSC_VER

#if __GNUC__ > 6
#pragma GCC diagnostic pop
#endif // __GNUC__ > 6

#endif // GSL_MULTI_SPAN_H
