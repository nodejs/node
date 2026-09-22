// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// To prevent compiler memory exhaustion (OOM / Killed signal terminates
// cc1plus) during parallel builds with GCC, the test suite instantiations have
// been split into multiple compilation units.

// SKIP_ABSL_INLINE_NAMESPACE_CHECK

#include "absl/functional/any_invocable_test.h"

namespace absl_any_invocable_test {

INSTANTIATE_TYPED_TEST_SUITE_P(NonRvalueCallNothrow, AnyInvTestBasic,
                               TestParameterListNonRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(CallNothrowRvalue, AnyInvTestBasic,
                               TestParameterListRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(NonRvalueCallNothrow, AnyInvTestCombinatoric,
                               TestParameterListNonRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(RvalueCallNothrow, AnyInvTestCombinatoric,
                               TestParameterListRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(NonRvalueCallNothrow, AnyInvTestMovable,
                               TestParameterListNonRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(RvalueCallNothrow, AnyInvTestMovable,
                               TestParameterListRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(NonRvalueCallNothrow, AnyInvTestNoexceptTrue,
                               TestParameterListNonRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(RvalueCallNothrow, AnyInvTestNoexceptTrue,
                               TestParameterListRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(NonRvalueCallNothrow, AnyInvTestNonRvalue,
                               TestParameterListNonRvalueQualifiersNothrowCall);

INSTANTIATE_TYPED_TEST_SUITE_P(CallNothrowRvalue, AnyInvTestRvalue,
                               TestParameterListRvalueQualifiersNothrowCall);

}  // namespace absl_any_invocable_test
