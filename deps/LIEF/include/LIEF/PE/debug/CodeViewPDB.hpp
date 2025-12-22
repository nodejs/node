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
#ifndef LIEF_PE_DEBUG_CODE_VIEW_PDB_H
#define LIEF_PE_DEBUG_CODE_VIEW_PDB_H
#include "LIEF/PE/debug/CodeView.hpp"

#include <cstdint>
#include <array>
#include <ostream>

namespace LIEF {
namespace PE {
class Parser;
class Builder;

namespace details {
struct pe_pdb_70;
struct pe_pdb_20;
}

/// CodeView PDB specialization
class LIEF_API CodeViewPDB : public CodeView {
  friend class Parser;
  friend class Builder;

  public:
  using signature_t = std::array<uint8_t, 16>;
  CodeViewPDB() :
    CodeView(CodeView::SIGNATURES::PDB_70)
  {}

  CodeViewPDB(std::string filename) :
    CodeView(CodeView::SIGNATURES::PDB_70),
    signature_{0},
    filename_(std::move(filename))
  {}

  CodeViewPDB(const details::pe_debug& debug_info,
              const details::pe_pdb_70& pdb_70, Section* sec);

  CodeViewPDB(const details::pe_debug& debug_info,
              const details::pe_pdb_20& pdb_70, Section* sec);

  CodeViewPDB(const CodeViewPDB& other) = default;
  CodeViewPDB& operator=(const CodeViewPDB& other) = default;

  CodeViewPDB(CodeViewPDB&& other) = default;
  CodeViewPDB& operator=(CodeViewPDB&& other) = default;

  /// The GUID signature to verify against the .pdb file signature.
  /// This attribute might be used to lookup remote PDB file on a symbol server
  std::string guid() const;

  /// Age value to verify. The age does not necessarily correspond to any known
  /// time value, it is used to determine if a .pdb file is out of sync with a corresponding .exe file.
  uint32_t age() const {
    return age_;
  }

  /// The 32-bit signature to verify against the .pdb file signature.
  const signature_t& signature() const {
    return signature_;
  }

  /// The path to the `.pdb` file
  const std::string& filename() const {
    return filename_;
  }

  void age(uint32_t age) {
    age_ = age;
  }

  void signature(const signature_t& sig) {
    signature_ = sig;
  }

  void filename(std::string filename) {
    filename_ = std::move(filename);
  }

  std::unique_ptr<Debug> clone() const override {
    return std::unique_ptr<Debug>(new CodeViewPDB(*this));
  }

  static bool classof(const Debug* debug) {
    if (!CodeView::classof(debug)) {
      return false;
    }
    const auto& cv = static_cast<const CodeView&>(*debug);

    return cv.signature() == SIGNATURES::PDB_20 ||
           cv.signature() == SIGNATURES::PDB_70;
  }

  void accept(Visitor& visitor) const override;

  std::string to_string() const override;

  ~CodeViewPDB() override = default;

  protected:
  uint32_t age_ = 0;
  signature_t signature_ = {0};
  std::string filename_;
};

} // namespace PE
} // namespace LIEF
#endif
