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
#ifndef LIEF_MACHO_ROUTINE_H
#define LIEF_MACHO_ROUTINE_H
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;

/// Class that represents the `LC_ROUTINE/LC_ROUTINE64` commands.
/// Accodring to the Mach-O `loader.h` documentation:
///
/// > The routines command contains the address of the dynamic shared library
/// > initialization routine and an index into the module table for the module
/// > that defines the routine.  Before any modules are used from the library the
/// > dynamic linker fully binds the module that defines the initialization routine
/// > and then calls it.  This gets called before any module initialization
/// > routines (used for C++ static constructors) in the library.
class LIEF_API Routine : public LoadCommand {
  friend class BinaryParser;
  public:
  Routine() = default;

  template<class T>
  LIEF_LOCAL Routine(const T& cmd);

  Routine& operator=(const Routine& copy) = default;
  Routine(const Routine& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<Routine>(new Routine(*this));
  }

  /// address of initialization routine
  uint64_t init_address() const {
    return init_address_;
  }

  void init_address(uint64_t addr) {
    init_address_ = addr;
  }

  /// Index into the module table that the init routine is defined in
  uint64_t init_module() const {
    return init_module_;
  }

  void init_module(uint64_t mod) {
    init_module_ = mod;
  }

  uint64_t reserved1() const {
    return reserved1_;
  }

  void reserved1(uint64_t value) {
    reserved1_ = value;
  }

  uint64_t reserved2() const {
    return reserved2_;
  }

  void reserved2(uint64_t value) {
    reserved2_ = value;
  }

  uint64_t reserved3() const {
    return reserved3_;
  }

  void reserved3(uint64_t value) {
    reserved3_ = value;
  }

  uint64_t reserved4() const {
    return reserved4_;
  }

  void reserved4(uint64_t value) {
    reserved4_ = value;
  }

  uint64_t reserved5() const {
    return reserved5_;
  }

  void reserved5(uint64_t value) {
    reserved5_ = value;
  }

  uint64_t reserved6() const {
    return reserved6_;
  }

  void reserved6(uint64_t value) {
    reserved6_ = value;
  }

  ~Routine() override = default;

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::ROUTINES ||
           cmd->command() == LoadCommand::TYPE::ROUTINES_64;
  }
  private:
  uint64_t init_address_ = 0;
  uint64_t init_module_ = 0;
  uint64_t reserved1_ = 0;
  uint64_t reserved2_ = 0;
  uint64_t reserved3_ = 0;
  uint64_t reserved4_ = 0;
  uint64_t reserved5_ = 0;
  uint64_t reserved6_ = 0;
};

}
}
#endif
