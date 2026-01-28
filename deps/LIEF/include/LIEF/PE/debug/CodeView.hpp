/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
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
#ifndef LIEF_PE_DEBUG_CODE_VIEW_H
#define LIEF_PE_DEBUG_CODE_VIEW_H
#include "LIEF/PE/debug/Debug.hpp"

namespace LIEF {
namespace PE {
class Parser;
class Builder;

/// Interface for the (generic) Debug CodeView (``IMAGE_DEBUG_TYPE_CODEVIEW``)
class LIEF_API CodeView : public Debug {
  friend class Parser;
  friend class Builder;

  public:
  /// Code view signatures
  /// @see: http://llvm.org/doxygen/CVDebugRecord_8h_source.html
  enum class SIGNATURES {
    UNKNOWN = 0,

    PDB_70 = 0x53445352, // RSDS
    PDB_20 = 0x3031424e, // NB10
    CV_50  = 0x3131424e, // NB11
    CV_41  = 0x3930424e, // NB09
  };

  CodeView() :
    Debug(Debug::TYPES::CODEVIEW)
  {}
  CodeView(SIGNATURES sig) :
    Debug{Debug::TYPES::CODEVIEW},
    sig_{sig}
  {}

  CodeView(const details::pe_debug& debug, SIGNATURES sig, Section* sec) :
    Debug(debug, sec),
    sig_(sig)
  {}

  CodeView(const CodeView& other) = default;
  CodeView& operator=(const CodeView& other) = default;

  CodeView(CodeView&& other) = default;
  CodeView& operator=(CodeView&& other) = default;

  ~CodeView() override = default;

  /// The signature that defines the underlying type of the payload
  SIGNATURES signature() const {
    return sig_;
  }

  std::unique_ptr<Debug> clone() const override {
    return std::unique_ptr<Debug>(new CodeView(*this));
  }

  static bool classof(const Debug* debug) {
    return debug->type() == Debug::TYPES::CODEVIEW;
  }

  std::string to_string() const override;

  void accept(Visitor& visitor) const override;

  protected:
  SIGNATURES sig_ = SIGNATURES::UNKNOWN;
};

LIEF_API const char* to_string(CodeView::SIGNATURES e);

} // namespace PE
} // namespace LIEF
#endif
