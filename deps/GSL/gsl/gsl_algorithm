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

#ifndef GSL_ALGORITHM_H
#define GSL_ALGORITHM_H

#include <gsl/gsl_assert> // for Expects
#include <gsl/span>       // for dynamic_extent, span

#include <algorithm>   // for copy_n
#include <cstddef>     // for ptrdiff_t
#include <type_traits> // for is_assignable

#ifdef _MSC_VER
#pragma warning(push)

// turn off some warnings that are noisy about our Expects statements
#pragma warning(disable : 4127) // conditional expression is constant
#pragma warning(disable : 4996) // unsafe use of std::copy_n

#endif // _MSC_VER

namespace gsl
{
// Note: this will generate faster code than std::copy using span iterator in older msvc+stl
// not necessary for msvc since VS2017 15.8 (_MSC_VER >= 1915)
template <class SrcElementType, std::ptrdiff_t SrcExtent, class DestElementType,
          std::ptrdiff_t DestExtent>
void copy(span<SrcElementType, SrcExtent> src, span<DestElementType, DestExtent> dest)
{
    static_assert(std::is_assignable<decltype(*dest.data()), decltype(*src.data())>::value,
                  "Elements of source span can not be assigned to elements of destination span");
    static_assert(SrcExtent == dynamic_extent || DestExtent == dynamic_extent ||
                      (SrcExtent <= DestExtent),
                  "Source range is longer than target range");

    Expects(dest.size() >= src.size());
    GSL_SUPPRESS(stl.1) // NO-FORMAT: attribute
    std::copy_n(src.data(), src.size(), dest.data());
}

} // namespace gsl

#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

#endif // GSL_ALGORITHM_H
