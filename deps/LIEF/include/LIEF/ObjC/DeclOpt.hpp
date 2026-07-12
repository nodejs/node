/* Copyright 2022 - 2025 R. Thomas
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
#ifndef LIEF_OBJC_DECL_OPT_H
#define LIEF_OBJC_DECL_OPT_H

namespace LIEF {
namespace objc {
/// This structure wraps options to tweak the generated output of
/// functions like LIEF::objc::Metadata::to_decl
struct DeclOpt {
  /// Whether annotations like method's address should be printed
  bool show_annotations = true;
};
}
}
#endif

