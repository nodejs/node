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
#ifndef LIEF_MACHO_DYLD_INFO_COMMAND_H
#define LIEF_MACHO_DYLD_INFO_COMMAND_H
#include <string>
#include <set>
#include <vector>
#include <ostream>
#include <memory>

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

#include "LIEF/MachO/LoadCommand.hpp"
#include "LIEF/MachO/type_traits.hpp"
#include "LIEF/iterators.hpp"

namespace LIEF {
class vector_iostream;
class BinaryStream;
namespace MachO {

class Binary;
class BinaryParser;
class BindingInfoIterator;
class Builder;
class DyldBindingInfo;
class ExportInfo;
class LinkEdit;
class RelocationDyld;

namespace details {
struct dyld_info_command;
}

/// Class that represents the LC_DYLD_INFO and LC_DYLD_INFO_ONLY commands
class LIEF_API DyldInfo : public LoadCommand {

  friend class BinaryParser;
  friend class Binary;
  friend class Builder;
  friend class LinkEdit;
  friend class BindingInfoIterator;

  public:
  /// Tuple of ``offset`` and ``size``
  using info_t = std::pair<uint32_t, uint32_t>;

  /// Internal container for storing DyldBindingInfo
  using binding_info_t = std::vector<std::unique_ptr<DyldBindingInfo>>;

  /// Iterator which outputs DyldBindingInfo&
  using it_binding_info = ref_iterator<binding_info_t&, DyldBindingInfo*>;

  /// Iterator which outputs const DyldBindingInfo&
  using it_const_binding_info = const_ref_iterator<const binding_info_t&, DyldBindingInfo*>;

  /// Internal container for storing ExportInfo
  using export_info_t = std::vector<std::unique_ptr<ExportInfo>>;

  /// Iterator which outputs const ExportInfo&
  using it_export_info = ref_iterator<export_info_t&, ExportInfo*>;

  /// Iterator which outputs const ExportInfo&
  using it_const_export_info = const_ref_iterator<const export_info_t&, ExportInfo*>;

  enum class BINDING_ENCODING_VERSION {
    UNKNOWN = 0,
    V1,
    V2
  };

  enum class REBASE_TYPE: uint64_t  {
    POINTER         = 1u,
    TEXT_ABSOLUTE32 = 2u,
    TEXT_PCREL32    = 3u,
    THREADED        = 102u,
  };

  enum class REBASE_OPCODES: uint8_t {
    DONE                               = 0x00u, ///< It's finished
    SET_TYPE_IMM                       = 0x10u, ///< Set type to immediate (lower 4-bits). Used for ordinal numbers from 0-15
    SET_SEGMENT_AND_OFFSET_ULEB        = 0x20u, ///< Set segment's index to immediate (lower 4-bits) and segment's offset to following ULEB128 encoding.
    ADD_ADDR_ULEB                      = 0x30u, ///< Add segment's offset with the following ULEB128 encoding.
    ADD_ADDR_IMM_SCALED                = 0x40u, ///< Add segment's offset with immediate scaling
    DO_REBASE_IMM_TIMES                = 0x50u, ///< Rebase in the range of ``[segment's offset; segment's offset + immediate * sizeof(ptr)]``
    DO_REBASE_ULEB_TIMES               = 0x60u, ///< Same as REBASE_OPCODE_DO_REBASE_IMM_TIMES but *immediate* is replaced with ULEB128 value
    DO_REBASE_ADD_ADDR_ULEB            = 0x70u, ///< Rebase and increment segment's offset with following ULEB128 encoding + pointer's size
    DO_REBASE_ULEB_TIMES_SKIPPING_ULEB = 0x80u  ///< Rebase and skip several bytes
  };

  /// Opcodes used by Dyld info to bind symbols
  enum class BIND_OPCODES: uint8_t {
    DONE                             = 0x00u, ///< It's finished
    SET_DYLIB_ORDINAL_IMM            = 0x10u, ///< Set ordinal to immediate (lower 4-bits). Used for ordinal numbers from 0-15
    SET_DYLIB_ORDINAL_ULEB           = 0x20u, ///< Set ordinal to following ULEB128 encoding. Used for ordinal numbers from 16+
    SET_DYLIB_SPECIAL_IMM            = 0x30u, ///< Set ordinal, with 0 or negative number as immediate. the value is sign extended.
    SET_SYMBOL_TRAILING_FLAGS_IMM    = 0x40u, ///< Set the following symbol (NULL-terminated char*).
    SET_TYPE_IMM                     = 0x50u, ///< Set the type to immediate (lower 4-bits). See BIND_TYPES
    SET_ADDEND_SLEB                  = 0x60u, ///< Set the addend field to the following SLEB128 encoding.
    SET_SEGMENT_AND_OFFSET_ULEB      = 0x70u, ///< Set Segment to immediate value, and address to the following SLEB128 encoding
    ADD_ADDR_ULEB                    = 0x80u, ///< Set the address field to the following SLEB128 encoding.
    DO_BIND                          = 0x90u, ///< Perform binding of current table row
    DO_BIND_ADD_ADDR_ULEB            = 0xA0u, ///< Perform binding, also add following ULEB128 as address
    DO_BIND_ADD_ADDR_IMM_SCALED      = 0xB0u, ///< Perform binding, also add immediate (lower 4-bits) using scaling
    DO_BIND_ULEB_TIMES_SKIPPING_ULEB = 0xC0u, ///< Perform binding for several symbols (as following ULEB128), and skip several bytes.
    THREADED                         = 0xD0u,

    THREADED_APPLY                            = 0xD0u | 0x01u,
    THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB = 0xD0u | 0x00u,
  };

  enum class BIND_SUBOPCODE_THREADED: uint8_t {
    SET_BIND_ORDINAL_TABLE_SIZE_ULEB = 0x00u,
    APPLY                            = 0x01u,
  };

  enum BIND_SYMBOL_FLAGS {
    WEAK_IMPORT = 0x1u,
    NON_WEAK_DEFINITION = 0x8u,
  };

  static constexpr auto OPCODE_MASK = uint32_t(0xF0);
  static constexpr auto IMMEDIATE_MASK = uint32_t(0x0F);

  DyldInfo();
  DyldInfo(const details::dyld_info_command& dyld_info_cmd);

  DyldInfo& operator=(DyldInfo other);
  DyldInfo(const DyldInfo& copy);

  void swap(DyldInfo& other) noexcept;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<DyldInfo>(new DyldInfo(*this));
  }

  ~DyldInfo() override;

  /// *Rebase* information
  ///
  /// Dyld rebases an image whenever dyld loads it at an address different
  /// from its preferred address.  The rebase information is a stream
  /// of byte sized opcodes for which symbolic names start with REBASE_OPCODE_.
  /// Conceptually the rebase information is a table of tuples:
  ///    <seg-index, seg-offset, type>
  /// The opcodes are a compressed way to encode the table by only
  /// encoding when a column changes.  In addition simple patterns
  /// like "every n'th offset for m times" can be encoded in a few
  /// bytes.
  ///
  /// @see ``/usr/include/mach-o/loader.h``
  const info_t& rebase() const {
    return rebase_;
  }

  /// Return Rebase's opcodes as raw data
  span<const uint8_t> rebase_opcodes() const {
    return rebase_opcodes_;
  }
  span<uint8_t> rebase_opcodes() {
    return rebase_opcodes_;
  }

  /// Set new opcodes
  void rebase_opcodes(buffer_t raw);

  /// Return the rebase opcodes in a humman-readable way
  std::string show_rebases_opcodes() const;

  /// *Bind* information
  ///
  /// Dyld binds an image during the loading process, if the image
  /// requires any pointers to be initialized to symbols in other images.
  /// The rebase information is a stream of byte sized
  /// opcodes for which symbolic names start with BIND_OPCODE_.
  /// Conceptually the bind information is a table of tuples:
  ///    <seg-index, seg-offset, type, symbol-library-ordinal, symbol-name, addend>
  /// The opcodes are a compressed way to encode the table by only
  /// encoding when a column changes.  In addition simple patterns
  /// like for runs of pointers initialzed to the same value can be
  /// encoded in a few bytes.
  ///
  /// @see ``/usr/include/mach-o/loader.h``
  const info_t& bind() const {
    return bind_;
  }

  /// Return Binding's opcodes as raw data
  span<const uint8_t> bind_opcodes() const {
    return bind_opcodes_;
  }
  span<uint8_t> bind_opcodes() {
    return bind_opcodes_;
  }

  /// Set new opcodes
  void bind_opcodes(buffer_t raw);

  /// Return the bind opcodes in a humman-readable way
  std::string show_bind_opcodes() const;

  /// *Weak Bind* information
  ///
  /// Some C++ programs require dyld to unique symbols so that all
  /// images in the process use the same copy of some code/data.
  /// This step is done after binding. The content of the weak_bind
  /// info is an opcode stream like the bind_info.  But it is sorted
  /// alphabetically by symbol name.  This enables dyld to walk
  /// all images with weak binding information in order and look
  /// for collisions.  If there are no collisions, dyld does
  /// no updating.  That means that some fixups are also encoded
  /// in the bind_info.  For instance, all calls to "operator new"
  /// are first bound to libstdc++.dylib using the information
  /// in bind_info.  Then if some image overrides operator new
  /// that is detected when the weak_bind information is processed
  /// and the call to operator new is then rebound.
  ///
  /// @see ``/usr/include/mach-o/loader.h``
  const info_t& weak_bind() const {
    return weak_bind_;
  }

  /// Return **Weak** Binding's opcodes as raw data
  span<const uint8_t> weak_bind_opcodes() const {
    return weak_bind_opcodes_;
  }
  span<uint8_t> weak_bind_opcodes() {
    return weak_bind_opcodes_;
  }

  /// Set new opcodes
  void weak_bind_opcodes(buffer_t raw);

  /// Return the bind opcodes in a humman-readable way
  std::string show_weak_bind_opcodes() const;

  /// *Lazy Bind* information
  ///
  /// Some uses of external symbols do not need to be bound immediately.
  /// Instead they can be lazily bound on first use.  The lazy_bind
  /// are contains a stream of BIND opcodes to bind all lazy symbols.
  /// Normal use is that dyld ignores the lazy_bind section when
  /// loading an image.  Instead the static linker arranged for the
  /// lazy pointer to initially point to a helper function which
  /// pushes the offset into the lazy_bind area for the symbol
  /// needing to be bound, then jumps to dyld which simply adds
  /// the offset to lazy_bind_off to get the information on what
  /// to bind.
  ///
  /// @see ``/usr/include/mach-o/loader.h``
  const info_t& lazy_bind() const {
    return lazy_bind_;
  }

  /// Return **Lazy** Binding's opcodes as raw data
  span<const uint8_t> lazy_bind_opcodes() const {
    return lazy_bind_opcodes_;
  }
  span<uint8_t> lazy_bind_opcodes() {
    return lazy_bind_opcodes_;
  }

  /// Set new opcodes
  void lazy_bind_opcodes(buffer_t raw);

  /// Return the lazy opcodes in a humman-readable way
  std::string show_lazy_bind_opcodes() const;

  /// Iterator over BindingInfo entries
  it_binding_info bindings() {
    return binding_info_;
  }

  it_const_binding_info bindings() const {
    return binding_info_;
  }

  /// *Export* information
  ///
  /// The symbols exported by a dylib are encoded in a trie.  This
  /// is a compact representation that factors out common prefixes.
  /// It also reduces LINKEDIT pages in RAM because it encodes all
  /// information (name, address, flags) in one small, contiguous range.
  /// The export area is a stream of nodes.  The first node sequentially
  /// is the start node for the trie.
  ///
  /// Nodes for a symbol start with a byte that is the length of
  /// the exported symbol information for the string so far.
  /// If there is no exported symbol, the byte is zero. If there
  /// is exported info, it follows the length byte.  The exported
  /// info normally consists of a flags and offset both encoded
  /// in uleb128.  The offset is location of the content named
  /// by the symbol.  It is the offset from the mach_header for
  /// the image.
  ///
  /// After the initial byte and optional exported symbol information
  /// is a byte of how many edges (0-255) that this node has leaving
  /// it, followed by each edge.
  /// Each edge is a zero terminated cstring of the addition chars
  /// in the symbol, followed by a uleb128 offset for the node that
  /// edge points to.
  ///
  /// @see ``/usr/include/mach-o/loader.h``
  const info_t& export_info() const {
    return export_;
  }

  /// Iterator over ExportInfo entries
  it_export_info exports() {
    return export_info_;
  }
  it_const_export_info exports() const {
    return export_info_;
  }

  /// Return Export's trie as raw data
  span<const uint8_t> export_trie() const {
    return export_trie_;
  }
  span<uint8_t> export_trie() {
    return export_trie_;
  }

  /// Set new trie
  void export_trie(buffer_t raw);

  /// Return the export trie in a humman-readable way
  std::string show_export_trie() const;

  void rebase(const info_t& info) {
    rebase_ = info;
  }
  void bind(const info_t& info) {
    bind_ = info;
  }
  void weak_bind(const info_t& info) {
    weak_bind_ = info;
  }
  void lazy_bind(const info_t& info) {
    lazy_bind_ = info;
  }
  void export_info(const info_t& info) {
    export_ = info;
  }

  void set_rebase_offset(uint32_t offset) {
    rebase_ = {offset, std::get<1>(rebase())};
  }
  void set_rebase_size(uint32_t size) {
  rebase_ = {std::get<0>(rebase()), size};
  }

  void set_bind_offset(uint32_t offset) {
    bind_ = {offset, std::get<1>(bind())};
  }
  void set_bind_size(uint32_t size) {
    bind_ = {std::get<0>(bind()), size};
  }

  void set_weak_bind_offset(uint32_t offset) {
    weak_bind_ = {offset, std::get<1>(weak_bind())};
  }
  void set_weak_bind_size(uint32_t size) {
    weak_bind_ = {std::get<0>(weak_bind()), size};
  }

  void set_lazy_bind_offset(uint32_t offset) {
    lazy_bind_ = {offset, std::get<1>(lazy_bind())};
  }
  void set_lazy_bind_size(uint32_t size) {
    lazy_bind_ = {std::get<0>(lazy_bind()), size};
  }

  void set_export_offset(uint32_t offset) {
    export_ = {offset, std::get<1>(export_info())};
  }

  void set_export_size(uint32_t size) {
    export_ = {std::get<0>(export_info()), size};
  }

  void add(std::unique_ptr<ExportInfo> info);

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    const LoadCommand::TYPE type = cmd->command();
    return type == LoadCommand::TYPE::DYLD_INFO ||
           type == LoadCommand::TYPE::DYLD_INFO_ONLY;
  }

  private:
  using bind_container_t = std::set<DyldBindingInfo*, std::function<bool(DyldBindingInfo*, DyldBindingInfo*)>>;

  LIEF_LOCAL void show_bindings(std::ostream& os, span<const uint8_t> buffer, bool is_lazy = false) const;

  LIEF_LOCAL void show_trie(std::ostream& output, std::string output_prefix,
                            BinaryStream& stream, uint64_t start, uint64_t end,
                            const std::string& prefix) const;

  LIEF_LOCAL DyldInfo& update_standard_bindings(const bind_container_t& bindings, vector_iostream& stream);
  LIEF_LOCAL DyldInfo& update_standard_bindings_v1(const bind_container_t& bindings, vector_iostream& stream);
  LIEF_LOCAL DyldInfo& update_standard_bindings_v2(const bind_container_t& bindings,
                                                   std::vector<RelocationDyld*> rebases, vector_iostream& stream);

  LIEF_LOCAL DyldInfo& update_weak_bindings(const bind_container_t& bindings, vector_iostream& stream);
  LIEF_LOCAL DyldInfo& update_lazy_bindings(const bind_container_t& bindings, vector_iostream& stream);

  LIEF_LOCAL DyldInfo& update_rebase_info(vector_iostream& stream);
  LIEF_LOCAL DyldInfo& update_binding_info(vector_iostream& stream, details::dyld_info_command& cmd);
  LIEF_LOCAL DyldInfo& update_export_trie(vector_iostream& stream);

  info_t   rebase_;
  span<uint8_t> rebase_opcodes_;

  info_t   bind_;
  span<uint8_t> bind_opcodes_;

  info_t   weak_bind_;
  span<uint8_t> weak_bind_opcodes_;

  info_t   lazy_bind_;
  span<uint8_t> lazy_bind_opcodes_;

  info_t   export_;
  span<uint8_t> export_trie_;

  export_info_t  export_info_;
  binding_info_t binding_info_;

  BINDING_ENCODING_VERSION binding_encoding_version_ = BINDING_ENCODING_VERSION::UNKNOWN;

  Binary* binary_ = nullptr;
};

LIEF_API const char* to_string(DyldInfo::REBASE_TYPE e);
LIEF_API const char* to_string(DyldInfo::REBASE_OPCODES e);
LIEF_API const char* to_string(DyldInfo::BIND_OPCODES e);
LIEF_API const char* to_string(DyldInfo::BIND_SUBOPCODE_THREADED e);


}
}
#endif
