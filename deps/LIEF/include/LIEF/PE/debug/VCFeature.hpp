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
#ifndef LIEF_PE_VC_FEATURE_H
#define LIEF_PE_VC_FEATURE_H

#include "LIEF/visibility.h"
#include "LIEF/PE/debug/Debug.hpp"

namespace LIEF {
namespace PE {

/// This class represents the `IMAGE_DEBUG_TYPE_VC_FEATURE` debug entry
class LIEF_API VCFeature : public Debug {
  public:
  static std::unique_ptr<VCFeature>
    parse(const details::pe_debug& hdr, Section* section, span<uint8_t> payload);

  VCFeature(const details::pe_debug& debug, Section* sec,
            uint32_t pre_vc, uint32_t c_cpp, uint32_t gs, uint32_t sdl,
            uint32_t guards) :
    Debug(debug, sec),
    pre_vc_(pre_vc), c_cpp_(c_cpp),
    gs_(gs), sdl_(sdl), guards_(guards)
  {}

  VCFeature(const VCFeature& other) = default;
  VCFeature& operator=(const VCFeature& other) = default;

  VCFeature(VCFeature&&) = default;
  VCFeature& operator=(VCFeature&& other) = default;

  std::unique_ptr<Debug> clone() const override {
    return std::unique_ptr<Debug>(new VCFeature(*this));
  }

  /// Count for `Pre-VC++ 11.00`
  uint32_t pre_vcpp() const {
    return pre_vc_;
  }

  /// Count for `C/C++`
  uint32_t c_cpp() const {
    return c_cpp_;
  }

  /// Count for `/GS` (number of guard stack)
  uint32_t gs() const {
    return gs_;
  }

  /// Whether `/sdl` was enabled for this binary.
  ///
  /// `sdl` stands for Security Development Lifecycle and provides enhanced
  /// security features like changing security-relevant warnings into errors or
  /// enforcing guard stack.
  uint32_t sdl() const {
    return sdl_;
  }

  /// Count for `/guardN`
  uint32_t guards() const {
    return guards_;
  }

  VCFeature& pre_vcpp(uint32_t value) {
    pre_vc_ = value;
    return *this;
  }

  VCFeature& c_cpp(uint32_t value) {
    c_cpp_ = value;
    return *this;
  }

  VCFeature& gs(uint32_t value) {
    gs_ = value;
    return *this;
  }

  VCFeature& sdl(uint32_t value) {
    sdl_ = value;
    return *this;
  }

  VCFeature& guards(uint32_t value) {
    guards_ = value;
    return *this;
  }

  static bool classof(const Debug* debug) {
    return debug->type() == Debug::TYPES::VC_FEATURE;
  }

  ~VCFeature() override = default;

  std::string to_string() const override;
  private:
  uint32_t pre_vc_ = 0;
  uint32_t c_cpp_ = 0;
  uint32_t gs_ = 0;
  uint32_t sdl_ = 0;
  uint32_t guards_ = 0;
};

}
}

#endif
