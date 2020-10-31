/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_UTIL_PROTOZERO_TO_TEXT_H_
#define SRC_TRACE_PROCESSOR_UTIL_PROTOZERO_TO_TEXT_H_

#include <string>

#include "perfetto/protozero/field.h"

namespace perfetto {
namespace trace_processor {

// Given a protozero message |protobytes| which is of fully qualified name
// |type|. We will convert this into a text proto format string.
//
// DebugProtozeroToText will use new lines between fields, and
// ShortDebugProtozeroToText will use only a single space.
std::string DebugProtozeroToText(const std::string& type,
                                 protozero::ConstBytes protobytes);
std::string ShortDebugProtozeroToText(const std::string& type,
                                      protozero::ConstBytes protobytes);

// Allow the conversion from a protozero enum to a string. The template is just
// to allow easy enum passing since we will do the explicit cast to a int32_t
// for the user.
std::string ProtozeroEnumToText(const std::string& type, int32_t enum_value);
template <typename Enum>
std::string ProtozeroEnumToText(const std::string& type, Enum enum_value) {
  return ProtozeroEnumToText(type, static_cast<int32_t>(enum_value));
}

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_UTIL_PROTOZERO_TO_TEXT_H_
