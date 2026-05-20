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
#ifndef LIEF_ELF_CORE_AUXV_H
#define LIEF_ELF_CORE_AUXV_H

#include <ostream>
#include <map>
#include <utility>

#include "LIEF/visibility.h"
#include "LIEF/ELF/enums.hpp"
#include "LIEF/ELF/Note.hpp"

namespace LIEF {
namespace ELF {

/// Class representing core auxv object
class LIEF_API CoreAuxv : public Note {
  public:
  enum class TYPE {
    END = 0,       /**< End of vector */
    IGNORE_TY,     /**< Entry should be ignored */
    EXECFD,        /**< File descriptor of program */
    PHDR,          /**< Program headers for program */
    PHENT,         /**< Size of program header entry */
    PHNUM,         /**< Number of program headers */
    PAGESZ,        /**< System page size */
    BASE,          /**< Base address of interpreter */
    FLAGS,         /**< Flags */
    ENTRY,         /**< Entry point of program */
    NOTELF,        /**< Program is not ELF */
    UID,           /**< Real uid */
    EUID,          /**< Effective uid */
    GID,           /**< Real gid */
    EGID,          /**< Effective gid */
    TGT_PLATFORM,  /**< String identifying platform.  */
    HWCAP,         /**< Machine dependent hints about processor capabilities.  */
    CLKTCK,        /**< Frequency of times() */
    FPUCW,         /**< Used FPU control word.  */
    DCACHEBSIZE,   /**< Data cache block size.  */
    ICACHEBSIZE,   /**< Instruction cache block size.  */
    UCACHEBSIZE,   /**< Instruction cache block size.  */
    IGNOREPPC,     /**< Entry should be ignored.  */
    SECURE,        /**< Boolean, was exec setuid-like?.  */
    BASE_PLATFORM, /**< String identifying real platform  */
    RANDOM,        /**< Address of 16 random bytes  */
    HWCAP2,        /**< Extension of AT_HWCAP  */
    //ENTRY27,
    //ENTRY28,
    //ENTRY29,
    //ENTRY30,
    EXECFN = 31,   /**< Filename of executable  */
    SYSINFO,       /**< Filename of executable  */
    SYSINFO_EHDR,  /**<  Pointer to ELF header of system-supplied DSO. */
  };

  CoreAuxv(ARCH arch, Header::CLASS cls, std::string name,
           uint32_t type, description_t description) :
    Note(std::move(name), Note::TYPE::CORE_AUXV, type, std::move(description), ""),
    arch_(arch), class_(cls)
  {}

  std::unique_ptr<Note> clone() const override {
    return std::unique_ptr<Note>(new CoreAuxv(*this));
  }

  /// A map of CoreAuxv::TYPE and the value
  std::map<TYPE, uint64_t> values() const;

  /// Return the value associated with the provided TYPE or
  /// a lief_errors::not_found if the type is not present.
  result<uint64_t> get(TYPE type) const;

  result<uint64_t> operator[](TYPE type) const {
    return get(type);
  }

  bool set(TYPE type, uint64_t value);
  bool set(const std::map<TYPE, uint64_t>& values);

  void dump(std::ostream& os) const override;

  void accept(Visitor& visitor) const override;

  static bool classof(const Note* note) {
    return note->type() == Note::TYPE::CORE_AUXV;
  }

  ~CoreAuxv() override = default;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const CoreAuxv& note) {
    note.dump(os);
    return os;
  }

  protected:
  ARCH arch_ = ARCH::NONE;
  Header::CLASS class_ = Header::CLASS::NONE;
};

LIEF_API const char* to_string(CoreAuxv::TYPE type);

} // namepsace ELF
} // namespace LIEF

#endif
