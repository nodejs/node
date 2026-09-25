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
#ifndef LIEF_ABSTRACT_BINARY_H
#define LIEF_ABSTRACT_BINARY_H

#include <vector>
#include <memory>
#include <unordered_map>

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/span.hpp"

#include "LIEF/Abstract/Header.hpp"
#include "LIEF/Abstract/Function.hpp"

#include "LIEF/asm/Instruction.hpp"
#include "LIEF/asm/AssemblerConfig.hpp"

namespace llvm {
class MCInst;
}

/// LIEF namespace
namespace LIEF {
class Section;
class Relocation;
class Symbol;

class DebugInfo;

namespace assembly {
class Engine;
}

/// Generic interface representing a binary executable.
///
/// This class provides a unified interface across multiple binary formats
/// such as ELF, PE, Mach-O, and others. It enables users to access binary
/// components like headers, sections, symbols, relocations,
/// and functions in a format-agnostic way.
///
/// Subclasses like LIEF::PE::Binary implement format-specific API
class LIEF_API Binary : public Object {
  public:

  /// Enumeration of virtual address types used for patching and memory access.
  enum class VA_TYPES {
    /// Automatically determine if the address is absolute or relative
    /// (default behavior).
    AUTO = 0,

    /// Relative Virtual Address (RVA), offset from image base.
    RVA = 1,

    /// Absolute Virtual Address.
    VA = 2
  };

  enum FORMATS {
    UNKNOWN = 0,
    ELF,
    PE,
    MACHO,
    OAT,
  };

  using functions_t = std::vector<Function>;

  /// Internal container
  using sections_t = std::vector<Section*>;

  /// Iterator that outputs LIEF::Section&
  using it_sections = ref_iterator<sections_t>;

  /// Iterator that outputs const LIEF::Section&
  using it_const_sections = const_ref_iterator<sections_t>;

  /// Internal container
  using symbols_t = std::vector<Symbol*>;

  /// Iterator that outputs LIEF::Symbol&
  using it_symbols = ref_iterator<symbols_t>;

  /// Iterator that outputs const LIEF::Symbol&
  using it_const_symbols = const_ref_iterator<symbols_t>;

  /// Internal container
  using relocations_t = std::vector<Relocation*>;

  /// Iterator that outputs LIEF::Relocation&
  using it_relocations = ref_iterator<relocations_t>;

  /// Iterator that outputs const LIEF::Relocation&
  using it_const_relocations = const_ref_iterator<relocations_t>;

  /// Instruction iterator
  using instructions_it = iterator_range<assembly::Instruction::Iterator>;

  public:
  Binary();
  Binary(FORMATS fmt);

  ~Binary() override;

  Binary& operator=(const Binary&) = delete;
  Binary(const Binary&) = delete;

  /// Executable format (ELF, PE, Mach-O) of the underlying binary
  FORMATS format() const {
    return format_;
  }

  /// Return the abstract header of the binary
  Header header() const {
    return get_abstract_header();
  }

  /// Return an iterator over the abstracted symbols in which the elements **can** be modified
  it_symbols symbols() {
    return get_abstract_symbols();
  }

  /// Return an iterator over the abstracted symbols in which the elements **can't** be modified
  it_const_symbols symbols() const {
    return const_cast<Binary*>(this)->get_abstract_symbols();
  }

  /// Check if a Symbol with the given name exists
  bool has_symbol(const std::string& name) const {
    return get_symbol(name) != nullptr;
  }

  /// Return the Symbol with the given name
  /// If the symbol does not exist, return a nullptr
  const Symbol* get_symbol(const std::string& name) const;

  Symbol* get_symbol(const std::string& name) {
    return const_cast<Symbol*>(static_cast<const Binary*>(this)->get_symbol(name));
  }

  /// Return an iterator over the binary's sections (LIEF::Section)
  it_sections sections() {
    return get_abstract_sections();
  }

  it_const_sections sections() const {
    return const_cast<Binary*>(this)->get_abstract_sections();
  }

  /// Remove **all** the sections in the underlying binary
  virtual void remove_section(const std::string& name, bool clear = false) = 0;

  /// Return an iterator over the binary relocation (LIEF::Relocation)
  it_relocations relocations() {
    return get_abstract_relocations();
  }

  it_const_relocations relocations() const {
    return const_cast<Binary*>(this)->get_abstract_relocations();
  }

  /// Binary's entrypoint (if any)
  virtual uint64_t entrypoint() const = 0;

  /// Binary's original size
  uint64_t original_size() const {
    return original_size_;
  }

  /// Return the functions exported by the binary
  functions_t exported_functions() const {
    return get_abstract_exported_functions();
  }

  /// Return libraries which are imported by the binary
  std::vector<std::string> imported_libraries() const {
    return get_abstract_imported_libraries();
  }

  /// Return functions imported by the binary
  functions_t imported_functions() const {
    return get_abstract_imported_functions();
  }

  /// Return the address of the given function name
  virtual result<uint64_t> get_function_address(const std::string& func_name) const;

  /// Method so that a ``visitor`` can visit us
  void accept(Visitor& visitor) const override;

  std::vector<uint64_t> xref(uint64_t address) const;

  /// Patch the content at virtual address @p address with @p patch_value
  ///
  /// @param[in] address        Address to patch
  /// @param[in] patch_value    Patch to apply
  /// @param[in] addr_type      Specify if the address should be used as an
  ///                           absolute virtual address or a RVA
  virtual void patch_address(uint64_t address, const std::vector<uint8_t>& patch_value,
                             VA_TYPES addr_type = VA_TYPES::AUTO) = 0;

  /// Patch the address with the given value
  ///
  /// @param[in] address      Address to patch
  /// @param[in] patch_value  Patch to apply
  /// @param[in] size         Size of the value in **bytes** (1, 2, ... 8)
  /// @param[in] addr_type    Specify if the address should be used as an absolute virtual address or an RVA
  virtual void patch_address(uint64_t address, uint64_t patch_value, size_t size = sizeof(uint64_t),
                             VA_TYPES addr_type = VA_TYPES::AUTO) = 0;

  /// Return the content located at the given virtual address
  virtual span<const uint8_t>
    get_content_from_virtual_address(uint64_t virtual_address, uint64_t size,
                                     VA_TYPES addr_type = VA_TYPES::AUTO) const = 0;

  /// Get the integer value at the given virtual address
  template<class T>
  LIEF::result<T> get_int_from_virtual_address(
    uint64_t va, VA_TYPES addr_type = VA_TYPES::AUTO) const
  {
    T value;
    static_assert(std::is_integral<T>::value, "Require an integral type");
    span<const uint8_t> raw = get_content_from_virtual_address(va, sizeof(T), addr_type);
    if (raw.empty() || raw.size() < sizeof(T)) {
      return make_error_code(lief_errors::read_error);
    }

    std::copy(raw.data(), raw.data() + sizeof(T),
              reinterpret_cast<uint8_t*>(&value));
    return value;
  }

  /// Change binary's original size.
  ///
  /// @warning
  /// This function should be used carefully as some optimizations
  /// can be performed with this value
  void original_size(uint64_t size) {
    original_size_ = size;
  }

  /// Check if the binary is position independent
  virtual bool is_pie() const = 0;

  /// Check if the binary uses ``NX`` protection
  virtual bool has_nx() const = 0;

  /// Default image base address if the ASLR is not enabled.
  virtual uint64_t imagebase() const = 0;

  /// Constructor functions that are called prior any other functions
  virtual functions_t ctor_functions() const = 0;

  /// Convert the given offset into a virtual address.
  ///
  /// @param[in] offset   The offset to convert.
  /// @param[in] slide    If not 0, it will replace the default base address (if any)
  virtual result<uint64_t> offset_to_virtual_address(uint64_t offset, uint64_t slide = 0) const = 0;

  virtual std::ostream& print(std::ostream& os) const {
    return os;
  }

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Binary& binary) {
    binary.print(os);
    return os;
  }

  /// Return the debug info if present. It can be either a
  /// LIEF::dwarf::DebugInfo or a LIEF::pdb::DebugInfo
  ///
  /// For ELF and Mach-O binaries, it returns the given DebugInfo object **only**
  /// if the binary embeds the DWARF debug info in the binary itself.
  ///
  /// For PE file, this function tries to find the **external** PDB using
  /// the LIEF::PE::CodeViewPDB::filename() output (if present). One can also
  /// use LIEF::pdb::load() or LIEF::pdb::DebugInfo::from_file() to get PDB debug
  /// info.
  ///
  /// \warning This function requires LIEF's extended version otherwise it
  /// **always** return a nullptr
  DebugInfo* debug_info() const;

  /// Disassemble code starting at the given virtual address and with the given
  /// size.
  ///
  /// ```cpp
  /// auto insts = binary->disassemble(0xacde, 100);
  /// for (std::unique_ptr<assembly::Instruction> inst : insts) {
  ///   std::cout << inst->to_string() << '\n';
  /// }
  /// ```
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(uint64_t address, size_t size) const;

  /// Disassemble code starting at the given virtual address
  ///
  /// ```cpp
  /// auto insts = binary->disassemble(0xacde);
  /// for (std::unique_ptr<assembly::Instruction> inst : insts) {
  ///   std::cout << inst->to_string() << '\n';
  /// }
  /// ```
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(uint64_t address) const;

  /// Disassemble code for the given symbol name
  ///
  /// ```cpp
  /// auto insts = binary->disassemble("__libc_start_main");
  /// for (std::unique_ptr<assembly::Instruction> inst : insts) {
  ///   std::cout << inst->to_string() << '\n';
  /// }
  /// ```
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(const std::string& function) const;

  /// Disassemble code provided by the given buffer at the specified
  /// `address` parameter.
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(const uint8_t* buffer, size_t size,
                              uint64_t address = 0) const;


  /// Disassemble code provided by the given vector of bytes at the specified
  /// `address` parameter.
  ///
  /// \see LIEF::assembly::Instruction
  instructions_it disassemble(const std::vector<uint8_t>& buffer,
                              uint64_t address = 0) const {
    return disassemble(buffer.data(), buffer.size(), address);
  }

  instructions_it disassemble(LIEF::span<const uint8_t> buffer,
                              uint64_t address = 0) const {
    return disassemble(buffer.data(), buffer.size(), address);
  }

  instructions_it disassemble(LIEF::span<uint8_t> buffer, uint64_t address = 0) const {
    return disassemble(buffer.data(), buffer.size(), address);
  }

  /// Assemble **and patch** the provided assembly code at the specified address.
  ///
  /// The function returns the generated assembly bytes
  ///
  /// ```cpp
  /// bin->assemble(0x12000440, R"asm(
  ///   xor rax, rbx;
  ///   mov rcx, rax;
  /// )asm");
  /// ```
  ///
  /// If you need to configure the assembly engine or to define addresses for
  /// symbols, you can provide your own assembly::AssemblerConfig.
  std::vector<uint8_t> assemble(uint64_t address, const std::string& Asm,
      assembly::AssemblerConfig& config = assembly::AssemblerConfig::default_config());

  /// Assemble **and patch** the address with the given LLVM MCInst.
  ///
  /// \warning Because of ABI compatibility, this MCInst can **only be used**
  ///          with the **same** version of LLVM used by LIEF (see documentation)
  std::vector<uint8_t> assemble(uint64_t address, const llvm::MCInst& inst);

  /// Assemble **and patch** the address with the given LLVM MCInst.
  ///
  /// \warning Because of ABI compatibility, this MCInst can **only be used**
  ///          with the **same** version of LLVM used by LIEF (see documentation)
  std::vector<uint8_t> assemble(uint64_t address,
                                const std::vector<llvm::MCInst>& insts);

  /// Get the default memory page size according to the architecture and
  /// the format of the current binary
  virtual uint64_t page_size() const;

  /// Load and associate an external debug file (e.g., DWARF or PDB) with this binary.
  ///
  /// This method attempts to load the debug information from the file located at the given path,
  /// and binds it to the current binary instance. If successful, it returns a pointer to the
  /// loaded DebugInfo object.
  ///
  /// \param path Path to the external debug file (e.g., `.dwarf`, `.pdb`)
  /// \return Pointer to the loaded DebugInfo object on success, or `nullptr` on failure.
  ///
  /// \warning It is the caller's responsibility to ensure that the debug file is
  ///          compatible with the binary. Incorrect associations may lead to
  ///          inconsistent or invalid results.
  ///
  /// \note This function does not verify that the debug file matches the binary's unique
  ///       identifier (e.g., build ID, GUID).
  DebugInfo* load_debug_info(const std::string& path);

  protected:
  FORMATS format_ = FORMATS::UNKNOWN;
  mutable std::unique_ptr<DebugInfo> debug_info_;
  mutable std::unordered_map<uint32_t, std::unique_ptr<assembly::Engine>> engines_;
  uint64_t original_size_ = 0;

  assembly::Engine* get_engine(uint64_t address) const;

  template<uint32_t Key, class F>
  LIEF_LOCAL assembly::Engine* get_cache_engine(uint64_t address, F&& f) const;

  // These functions need to be overloaded by the object that claims to extend this Abstract Binary
  virtual Header get_abstract_header() const = 0;
  virtual symbols_t get_abstract_symbols() = 0;
  virtual sections_t get_abstract_sections() = 0;
  virtual relocations_t get_abstract_relocations() = 0;

  virtual functions_t  get_abstract_exported_functions() const = 0;
  virtual functions_t  get_abstract_imported_functions() const = 0;
  virtual std::vector<std::string> get_abstract_imported_libraries() const = 0;
};

LIEF_API const char* to_string(Binary::VA_TYPES e);
LIEF_API const char* to_string(Binary::FORMATS e);

}


#endif
