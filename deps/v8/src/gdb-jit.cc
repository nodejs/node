// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef ENABLE_GDB_JIT_INTERFACE
#include "gdb-jit.h"

#include "bootstrapper.h"
#include "compiler.h"
#include "global-handles.h"
#include "messages.h"
#include "natives.h"

namespace v8 {
namespace internal {

class ELF;

class Writer BASE_EMBEDDED {
 public:
  explicit Writer(ELF* elf)
      : elf_(elf),
        position_(0),
        capacity_(1024),
        buffer_(reinterpret_cast<byte*>(malloc(capacity_))) {
  }

  ~Writer() {
    free(buffer_);
  }

  uintptr_t position() const {
    return position_;
  }

  template<typename T>
  class Slot {
   public:
    Slot(Writer* w, uintptr_t offset) : w_(w), offset_(offset) { }

    T* operator-> () {
      return w_->RawSlotAt<T>(offset_);
    }

    void set(const T& value) {
      *w_->RawSlotAt<T>(offset_) = value;
    }

    Slot<T> at(int i) {
      return Slot<T>(w_, offset_ + sizeof(T) * i);
    }

   private:
    Writer* w_;
    uintptr_t offset_;
  };

  template<typename T>
  void Write(const T& val) {
    Ensure(position_ + sizeof(T));
    *RawSlotAt<T>(position_) = val;
    position_ += sizeof(T);
  }

  template<typename T>
  Slot<T> SlotAt(uintptr_t offset) {
    Ensure(offset + sizeof(T));
    return Slot<T>(this, offset);
  }

  template<typename T>
  Slot<T> CreateSlotHere() {
    return CreateSlotsHere<T>(1);
  }

  template<typename T>
  Slot<T> CreateSlotsHere(uint32_t count) {
    uintptr_t slot_position = position_;
    position_ += sizeof(T) * count;
    Ensure(position_);
    return SlotAt<T>(slot_position);
  }

  void Ensure(uintptr_t pos) {
    if (capacity_ < pos) {
      while (capacity_ < pos) capacity_ *= 2;
      buffer_ = reinterpret_cast<byte*>(realloc(buffer_, capacity_));
    }
  }

  ELF* elf() { return elf_; }

  byte* buffer() { return buffer_; }

  void Align(uintptr_t align) {
    uintptr_t delta = position_ % align;
    if (delta == 0) return;
    uintptr_t padding = align - delta;
    Ensure(position_ += padding);
    ASSERT((position_ % align) == 0);
  }

  void WriteULEB128(uintptr_t value) {
    do {
      uint8_t byte = value & 0x7F;
      value >>= 7;
      if (value != 0) byte |= 0x80;
      Write<uint8_t>(byte);
    } while (value != 0);
  }

  void WriteSLEB128(intptr_t value) {
    bool more = true;
    while (more) {
      int8_t byte = value & 0x7F;
      bool byte_sign = byte & 0x40;
      value >>= 7;

      if ((value == 0 && !byte_sign) || (value == -1 && byte_sign)) {
        more = false;
      } else {
        byte |= 0x80;
      }

      Write<int8_t>(byte);
    }
  }

  void WriteString(const char* str) {
    do {
      Write<char>(*str);
    } while (*str++);
  }

 private:
  template<typename T> friend class Slot;

  template<typename T>
  T* RawSlotAt(uintptr_t offset) {
    ASSERT(offset < capacity_ && offset + sizeof(T) <= capacity_);
    return reinterpret_cast<T*>(&buffer_[offset]);
  }

  ELF* elf_;
  uintptr_t position_;
  uintptr_t capacity_;
  byte* buffer_;
};

class StringTable;

class ELFSection : public ZoneObject {
 public:
  struct Header {
    uint32_t name;
    uint32_t type;
    uintptr_t flags;
    uintptr_t address;
    uintptr_t offset;
    uintptr_t size;
    uint32_t link;
    uint32_t info;
    uintptr_t alignment;
    uintptr_t entry_size;
  };

  enum Type {
    TYPE_NULL = 0,
    TYPE_PROGBITS = 1,
    TYPE_SYMTAB = 2,
    TYPE_STRTAB = 3,
    TYPE_RELA = 4,
    TYPE_HASH = 5,
    TYPE_DYNAMIC = 6,
    TYPE_NOTE = 7,
    TYPE_NOBITS = 8,
    TYPE_REL = 9,
    TYPE_SHLIB = 10,
    TYPE_DYNSYM = 11,
    TYPE_LOPROC = 0x70000000,
    TYPE_X86_64_UNWIND = 0x70000001,
    TYPE_HIPROC = 0x7fffffff,
    TYPE_LOUSER = 0x80000000,
    TYPE_HIUSER = 0xffffffff
  };

  enum Flags {
    FLAG_WRITE = 1,
    FLAG_ALLOC = 2,
    FLAG_EXEC = 4
  };

  enum SpecialIndexes {
    INDEX_ABSOLUTE = 0xfff1
  };

  ELFSection(const char* name, Type type, uintptr_t align)
      : name_(name), type_(type), align_(align) { }

  virtual ~ELFSection() { }

  void PopulateHeader(Writer::Slot<Header> header, StringTable* strtab);

  virtual void WriteBody(Writer::Slot<Header> header, Writer* w) {
    uintptr_t start = w->position();
    if (WriteBody(w)) {
      uintptr_t end = w->position();
      header->offset = start;
      header->size = end - start;
    }
  }

  virtual bool WriteBody(Writer* w) {
    return false;
  }

  uint16_t index() const { return index_; }
  void set_index(uint16_t index) { index_ = index; }

 protected:
  virtual void PopulateHeader(Writer::Slot<Header> header) {
    header->flags = 0;
    header->address = 0;
    header->offset = 0;
    header->size = 0;
    header->link = 0;
    header->info = 0;
    header->entry_size = 0;
  }


 private:
  const char* name_;
  Type type_;
  uintptr_t align_;
  uint16_t index_;
};


class FullHeaderELFSection : public ELFSection {
 public:
  FullHeaderELFSection(const char* name,
                       Type type,
                       uintptr_t align,
                       uintptr_t addr,
                       uintptr_t offset,
                       uintptr_t size,
                       uintptr_t flags)
      : ELFSection(name, type, align),
        addr_(addr),
        offset_(offset),
        size_(size),
        flags_(flags) { }

 protected:
  virtual void PopulateHeader(Writer::Slot<Header> header) {
    ELFSection::PopulateHeader(header);
    header->address = addr_;
    header->offset = offset_;
    header->size = size_;
    header->flags = flags_;
  }

 private:
  uintptr_t addr_;
  uintptr_t offset_;
  uintptr_t size_;
  uintptr_t flags_;
};


class StringTable : public ELFSection {
 public:
  explicit StringTable(const char* name)
      : ELFSection(name, TYPE_STRTAB, 1), writer_(NULL), offset_(0), size_(0) {
  }

  uintptr_t Add(const char* str) {
    if (*str == '\0') return 0;

    uintptr_t offset = size_;
    WriteString(str);
    return offset;
  }

  void AttachWriter(Writer* w) {
    writer_ = w;
    offset_ = writer_->position();

    // First entry in the string table should be an empty string.
    WriteString("");
  }

  void DetachWriter() {
    writer_ = NULL;
  }

  virtual void WriteBody(Writer::Slot<Header> header, Writer* w) {
    ASSERT(writer_ == NULL);
    header->offset = offset_;
    header->size = size_;
  }

 private:
  void WriteString(const char* str) {
    uintptr_t written = 0;
    do {
      writer_->Write(*str);
      written++;
    } while (*str++);
    size_ += written;
  }

  Writer* writer_;

  uintptr_t offset_;
  uintptr_t size_;
};


void ELFSection::PopulateHeader(Writer::Slot<ELFSection::Header> header,
                                StringTable* strtab) {
  header->name = strtab->Add(name_);
  header->type = type_;
  header->alignment = align_;
  PopulateHeader(header);
}


class ELF BASE_EMBEDDED {
 public:
  ELF() : sections_(6) {
    sections_.Add(new ELFSection("", ELFSection::TYPE_NULL, 0));
    sections_.Add(new StringTable(".shstrtab"));
  }

  void Write(Writer* w) {
    WriteHeader(w);
    WriteSectionTable(w);
    WriteSections(w);
  }

  ELFSection* SectionAt(uint32_t index) {
    return sections_[index];
  }

  uint32_t AddSection(ELFSection* section) {
    sections_.Add(section);
    section->set_index(sections_.length() - 1);
    return sections_.length() - 1;
  }

 private:
  struct ELFHeader {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uintptr_t entry;
    uintptr_t pht_offset;
    uintptr_t sht_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t pht_entry_size;
    uint16_t pht_entry_num;
    uint16_t sht_entry_size;
    uint16_t sht_entry_num;
    uint16_t sht_strtab_index;
  };


  void WriteHeader(Writer* w) {
    ASSERT(w->position() == 0);
    Writer::Slot<ELFHeader> header = w->CreateSlotHere<ELFHeader>();
#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_ARM)
    const uint8_t ident[16] =
        { 0x7f, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#elif defined(V8_TARGET_ARCH_X64)
    const uint8_t ident[16] =
        { 0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0 , 0, 0, 0, 0, 0, 0};
#else
#error Unsupported target architecture.
#endif
    memcpy(header->ident, ident, 16);
    header->type = 1;
#if defined(V8_TARGET_ARCH_IA32)
    header->machine = 3;
#elif defined(V8_TARGET_ARCH_X64)
    // Processor identification value for x64 is 62 as defined in
    //    System V ABI, AMD64 Supplement
    //    http://www.x86-64.org/documentation/abi.pdf
    header->machine = 62;
#elif defined(V8_TARGET_ARCH_ARM)
    // Set to EM_ARM, defined as 40, in "ARM ELF File Format" at
    // infocenter.arm.com/help/topic/com.arm.doc.dui0101a/DUI0101A_Elf.pdf
    header->machine = 40;
#else
#error Unsupported target architecture.
#endif
    header->version = 1;
    header->entry = 0;
    header->pht_offset = 0;
    header->sht_offset = sizeof(ELFHeader);  // Section table follows header.
    header->flags = 0;
    header->header_size = sizeof(ELFHeader);
    header->pht_entry_size = 0;
    header->pht_entry_num = 0;
    header->sht_entry_size = sizeof(ELFSection::Header);
    header->sht_entry_num = sections_.length();
    header->sht_strtab_index = 1;
  }

  void WriteSectionTable(Writer* w) {
    // Section headers table immediately follows file header.
    ASSERT(w->position() == sizeof(ELFHeader));

    Writer::Slot<ELFSection::Header> headers =
        w->CreateSlotsHere<ELFSection::Header>(sections_.length());

    // String table for section table is the first section.
    StringTable* strtab = static_cast<StringTable*>(SectionAt(1));
    strtab->AttachWriter(w);
    for (int i = 0, length = sections_.length();
         i < length;
         i++) {
      sections_[i]->PopulateHeader(headers.at(i), strtab);
    }
    strtab->DetachWriter();
  }

  int SectionHeaderPosition(uint32_t section_index) {
    return sizeof(ELFHeader) + sizeof(ELFSection::Header) * section_index;
  }

  void WriteSections(Writer* w) {
    Writer::Slot<ELFSection::Header> headers =
        w->SlotAt<ELFSection::Header>(sizeof(ELFHeader));

    for (int i = 0, length = sections_.length();
         i < length;
         i++) {
      sections_[i]->WriteBody(headers.at(i), w);
    }
  }

  ZoneList<ELFSection*> sections_;
};


class ELFSymbol BASE_EMBEDDED {
 public:
  enum Type {
    TYPE_NOTYPE = 0,
    TYPE_OBJECT = 1,
    TYPE_FUNC = 2,
    TYPE_SECTION = 3,
    TYPE_FILE = 4,
    TYPE_LOPROC = 13,
    TYPE_HIPROC = 15
  };

  enum Binding {
    BIND_LOCAL = 0,
    BIND_GLOBAL = 1,
    BIND_WEAK = 2,
    BIND_LOPROC = 13,
    BIND_HIPROC = 15
  };

  ELFSymbol(const char* name,
            uintptr_t value,
            uintptr_t size,
            Binding binding,
            Type type,
            uint16_t section)
      : name(name),
        value(value),
        size(size),
        info((binding << 4) | type),
        other(0),
        section(section) {
  }

  Binding binding() const {
    return static_cast<Binding>(info >> 4);
  }
#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_ARM)
  struct SerializedLayout {
    SerializedLayout(uint32_t name,
                     uintptr_t value,
                     uintptr_t size,
                     Binding binding,
                     Type type,
                     uint16_t section)
        : name(name),
          value(value),
          size(size),
          info((binding << 4) | type),
          other(0),
          section(section) {
    }

    uint32_t name;
    uintptr_t value;
    uintptr_t size;
    uint8_t info;
    uint8_t other;
    uint16_t section;
  };
#elif defined(V8_TARGET_ARCH_X64)
  struct SerializedLayout {
    SerializedLayout(uint32_t name,
                     uintptr_t value,
                     uintptr_t size,
                     Binding binding,
                     Type type,
                     uint16_t section)
        : name(name),
          info((binding << 4) | type),
          other(0),
          section(section),
          value(value),
          size(size) {
    }

    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t section;
    uintptr_t value;
    uintptr_t size;
  };
#endif

  void Write(Writer::Slot<SerializedLayout> s, StringTable* t) {
    // Convert symbol names from strings to indexes in the string table.
    s->name = t->Add(name);
    s->value = value;
    s->size = size;
    s->info = info;
    s->other = other;
    s->section = section;
  }

 private:
  const char* name;
  uintptr_t value;
  uintptr_t size;
  uint8_t info;
  uint8_t other;
  uint16_t section;
};


class ELFSymbolTable : public ELFSection {
 public:
  explicit ELFSymbolTable(const char* name)
      : ELFSection(name, TYPE_SYMTAB, sizeof(uintptr_t)),
        locals_(1),
        globals_(1) {
  }

  virtual void WriteBody(Writer::Slot<Header> header, Writer* w) {
    w->Align(header->alignment);
    int total_symbols = locals_.length() + globals_.length() + 1;
    header->offset = w->position();

    Writer::Slot<ELFSymbol::SerializedLayout> symbols =
        w->CreateSlotsHere<ELFSymbol::SerializedLayout>(total_symbols);

    header->size = w->position() - header->offset;

    // String table for this symbol table should follow it in the section table.
    StringTable* strtab =
        static_cast<StringTable*>(w->elf()->SectionAt(index() + 1));
    strtab->AttachWriter(w);
    symbols.at(0).set(ELFSymbol::SerializedLayout(0,
                                                  0,
                                                  0,
                                                  ELFSymbol::BIND_LOCAL,
                                                  ELFSymbol::TYPE_NOTYPE,
                                                  0));
    WriteSymbolsList(&locals_, symbols.at(1), strtab);
    WriteSymbolsList(&globals_, symbols.at(locals_.length() + 1), strtab);
    strtab->DetachWriter();
  }

  void Add(const ELFSymbol& symbol) {
    if (symbol.binding() == ELFSymbol::BIND_LOCAL) {
      locals_.Add(symbol);
    } else {
      globals_.Add(symbol);
    }
  }

 protected:
  virtual void PopulateHeader(Writer::Slot<Header> header) {
    ELFSection::PopulateHeader(header);
    // We are assuming that string table will follow symbol table.
    header->link = index() + 1;
    header->info = locals_.length() + 1;
    header->entry_size = sizeof(ELFSymbol::SerializedLayout);
  }

 private:
  void WriteSymbolsList(const ZoneList<ELFSymbol>* src,
                        Writer::Slot<ELFSymbol::SerializedLayout> dst,
                        StringTable* strtab) {
    for (int i = 0, len = src->length();
         i < len;
         i++) {
      src->at(i).Write(dst.at(i), strtab);
    }
  }

  ZoneList<ELFSymbol> locals_;
  ZoneList<ELFSymbol> globals_;
};


class CodeDescription BASE_EMBEDDED {
 public:

#ifdef V8_TARGET_ARCH_X64
  enum StackState {
    POST_RBP_PUSH,
    POST_RBP_SET,
    POST_RBP_POP,
    STACK_STATE_MAX
  };
#endif

  CodeDescription(const char* name,
                  Code* code,
                  Handle<Script> script,
                  GDBJITLineInfo* lineinfo,
                  GDBJITInterface::CodeTag tag)
      : name_(name),
        code_(code),
        script_(script),
        lineinfo_(lineinfo),
        tag_(tag) {
  }

  const char* name() const {
    return name_;
  }

  GDBJITLineInfo* lineinfo() const {
    return lineinfo_;
  }

  GDBJITInterface::CodeTag tag() const {
    return tag_;
  }

  uintptr_t CodeStart() const {
    return reinterpret_cast<uintptr_t>(code_->instruction_start());
  }

  uintptr_t CodeEnd() const {
    return reinterpret_cast<uintptr_t>(code_->instruction_end());
  }

  uintptr_t CodeSize() const {
    return CodeEnd() - CodeStart();
  }

  bool IsLineInfoAvailable() {
    return !script_.is_null() &&
        script_->source()->IsString() &&
        script_->HasValidSource() &&
        script_->name()->IsString() &&
        lineinfo_ != NULL;
  }

#ifdef V8_TARGET_ARCH_X64
  uintptr_t GetStackStateStartAddress(StackState state) const {
    ASSERT(state < STACK_STATE_MAX);
    return stack_state_start_addresses_[state];
  }

  void SetStackStateStartAddress(StackState state, uintptr_t addr) {
    ASSERT(state < STACK_STATE_MAX);
    stack_state_start_addresses_[state] = addr;
  }
#endif

  SmartPointer<char> GetFilename() {
    return String::cast(script_->name())->ToCString();
  }

  int GetScriptLineNumber(int pos) {
    return GetScriptLineNumberSafe(script_, pos) + 1;
  }


 private:
  const char* name_;
  Code* code_;
  Handle<Script> script_;
  GDBJITLineInfo* lineinfo_;
  GDBJITInterface::CodeTag tag_;
#ifdef V8_TARGET_ARCH_X64
  uintptr_t stack_state_start_addresses_[STACK_STATE_MAX];
#endif
};


static void CreateSymbolsTable(CodeDescription* desc,
                               ELF* elf,
                               int text_section_index) {
  ELFSymbolTable* symtab = new ELFSymbolTable(".symtab");
  StringTable* strtab = new StringTable(".strtab");

  // Symbol table should be followed by the linked string table.
  elf->AddSection(symtab);
  elf->AddSection(strtab);

  symtab->Add(ELFSymbol("V8 Code",
                        0,
                        0,
                        ELFSymbol::BIND_LOCAL,
                        ELFSymbol::TYPE_FILE,
                        ELFSection::INDEX_ABSOLUTE));

  symtab->Add(ELFSymbol(desc->name(),
                        0,
                        desc->CodeSize(),
                        ELFSymbol::BIND_GLOBAL,
                        ELFSymbol::TYPE_FUNC,
                        text_section_index));
}


class DebugInfoSection : public ELFSection {
 public:
  explicit DebugInfoSection(CodeDescription* desc)
      : ELFSection(".debug_info", TYPE_PROGBITS, 1), desc_(desc) { }

  bool WriteBody(Writer* w) {
    Writer::Slot<uint32_t> size = w->CreateSlotHere<uint32_t>();
    uintptr_t start = w->position();
    w->Write<uint16_t>(2);  // DWARF version.
    w->Write<uint32_t>(0);  // Abbreviation table offset.
    w->Write<uint8_t>(sizeof(intptr_t));

    w->WriteULEB128(1);  // Abbreviation code.
    w->WriteString(*desc_->GetFilename());
    w->Write<intptr_t>(desc_->CodeStart());
    w->Write<intptr_t>(desc_->CodeStart() + desc_->CodeSize());
    w->Write<uint32_t>(0);
    size.set(static_cast<uint32_t>(w->position() - start));
    return true;
  }

 private:
  CodeDescription* desc_;
};


class DebugAbbrevSection : public ELFSection {
 public:
  DebugAbbrevSection() : ELFSection(".debug_abbrev", TYPE_PROGBITS, 1) { }

  // DWARF2 standard, figure 14.
  enum DWARF2Tags {
    DW_TAG_COMPILE_UNIT = 0x11
  };

  // DWARF2 standard, figure 16.
  enum DWARF2ChildrenDetermination {
    DW_CHILDREN_NO = 0,
    DW_CHILDREN_YES = 1
  };

  // DWARF standard, figure 17.
  enum DWARF2Attribute {
    DW_AT_NAME = 0x3,
    DW_AT_STMT_LIST = 0x10,
    DW_AT_LOW_PC = 0x11,
    DW_AT_HIGH_PC = 0x12
  };

  // DWARF2 standard, figure 19.
  enum DWARF2AttributeForm {
    DW_FORM_ADDR = 0x1,
    DW_FORM_STRING = 0x8,
    DW_FORM_DATA4 = 0x6
  };

  bool WriteBody(Writer* w) {
    w->WriteULEB128(1);
    w->WriteULEB128(DW_TAG_COMPILE_UNIT);
    w->Write<uint8_t>(DW_CHILDREN_NO);
    w->WriteULEB128(DW_AT_NAME);
    w->WriteULEB128(DW_FORM_STRING);
    w->WriteULEB128(DW_AT_LOW_PC);
    w->WriteULEB128(DW_FORM_ADDR);
    w->WriteULEB128(DW_AT_HIGH_PC);
    w->WriteULEB128(DW_FORM_ADDR);
    w->WriteULEB128(DW_AT_STMT_LIST);
    w->WriteULEB128(DW_FORM_DATA4);
    w->WriteULEB128(0);
    w->WriteULEB128(0);
    w->WriteULEB128(0);
    return true;
  }
};


class DebugLineSection : public ELFSection {
 public:
  explicit DebugLineSection(CodeDescription* desc)
      : ELFSection(".debug_line", TYPE_PROGBITS, 1),
        desc_(desc) { }

  // DWARF2 standard, figure 34.
  enum DWARF2Opcodes {
    DW_LNS_COPY = 1,
    DW_LNS_ADVANCE_PC = 2,
    DW_LNS_ADVANCE_LINE = 3,
    DW_LNS_SET_FILE = 4,
    DW_LNS_SET_COLUMN = 5,
    DW_LNS_NEGATE_STMT = 6
  };

  // DWARF2 standard, figure 35.
  enum DWARF2ExtendedOpcode {
    DW_LNE_END_SEQUENCE = 1,
    DW_LNE_SET_ADDRESS = 2,
    DW_LNE_DEFINE_FILE = 3
  };

  bool WriteBody(Writer* w) {
    // Write prologue.
    Writer::Slot<uint32_t> total_length = w->CreateSlotHere<uint32_t>();
    uintptr_t start = w->position();

    // Used for special opcodes
    const int8_t line_base = 1;
    const uint8_t line_range = 7;
    const int8_t max_line_incr = (line_base + line_range - 1);
    const uint8_t opcode_base = DW_LNS_NEGATE_STMT + 1;

    w->Write<uint16_t>(2);  // Field version.
    Writer::Slot<uint32_t> prologue_length = w->CreateSlotHere<uint32_t>();
    uintptr_t prologue_start = w->position();
    w->Write<uint8_t>(1);  // Field minimum_instruction_length.
    w->Write<uint8_t>(1);  // Field default_is_stmt.
    w->Write<int8_t>(line_base);  // Field line_base.
    w->Write<uint8_t>(line_range);  // Field line_range.
    w->Write<uint8_t>(opcode_base);  // Field opcode_base.
    w->Write<uint8_t>(0);  // DW_LNS_COPY operands count.
    w->Write<uint8_t>(1);  // DW_LNS_ADVANCE_PC operands count.
    w->Write<uint8_t>(1);  // DW_LNS_ADVANCE_LINE operands count.
    w->Write<uint8_t>(1);  // DW_LNS_SET_FILE operands count.
    w->Write<uint8_t>(1);  // DW_LNS_SET_COLUMN operands count.
    w->Write<uint8_t>(0);  // DW_LNS_NEGATE_STMT operands count.
    w->Write<uint8_t>(0);  // Empty include_directories sequence.
    w->WriteString(*desc_->GetFilename());  // File name.
    w->WriteULEB128(0);  // Current directory.
    w->WriteULEB128(0);  // Unknown modification time.
    w->WriteULEB128(0);  // Unknown file size.
    w->Write<uint8_t>(0);
    prologue_length.set(static_cast<uint32_t>(w->position() - prologue_start));

    WriteExtendedOpcode(w, DW_LNE_SET_ADDRESS, sizeof(intptr_t));
    w->Write<intptr_t>(desc_->CodeStart());
    w->Write<uint8_t>(DW_LNS_COPY);

    intptr_t pc = 0;
    intptr_t line = 1;
    bool is_statement = true;

    List<GDBJITLineInfo::PCInfo>* pc_info = desc_->lineinfo()->pc_info();
    pc_info->Sort(&ComparePCInfo);

    int pc_info_length = pc_info->length();
    for (int i = 0; i < pc_info_length; i++) {
      GDBJITLineInfo::PCInfo* info = &pc_info->at(i);
      ASSERT(info->pc_ >= pc);

      // Reduce bloating in the debug line table by removing duplicate line
      // entries (per DWARF2 standard).
      intptr_t  new_line = desc_->GetScriptLineNumber(info->pos_);
      if (new_line == line) {
        continue;
      }

      // Mark statement boundaries.  For a better debugging experience, mark
      // the last pc address in the function as a statement (e.g. "}"), so that
      // a user can see the result of the last line executed in the function,
      // should control reach the end.
      if ((i+1) == pc_info_length) {
        if (!is_statement) {
          w->Write<uint8_t>(DW_LNS_NEGATE_STMT);
        }
      } else if (is_statement != info->is_statement_) {
        w->Write<uint8_t>(DW_LNS_NEGATE_STMT);
        is_statement = !is_statement;
      }

      // Generate special opcodes, if possible.  This results in more compact
      // debug line tables.  See the DWARF 2.0 standard to learn more about
      // special opcodes.
      uintptr_t pc_diff = info->pc_ - pc;
      intptr_t line_diff = new_line - line;

      // Compute special opcode (see DWARF 2.0 standard)
      intptr_t special_opcode = (line_diff - line_base) +
                                (line_range * pc_diff) + opcode_base;

      // If special_opcode is less than or equal to 255, it can be used as a
      // special opcode.  If line_diff is larger than the max line increment
      // allowed for a special opcode, or if line_diff is less than the minimum
      // line that can be added to the line register (i.e. line_base), then
      // special_opcode can't be used.
      if ((special_opcode >= opcode_base) && (special_opcode <= 255) &&
          (line_diff <= max_line_incr) && (line_diff >= line_base)) {
        w->Write<uint8_t>(special_opcode);
      } else {
        w->Write<uint8_t>(DW_LNS_ADVANCE_PC);
        w->WriteSLEB128(pc_diff);
        w->Write<uint8_t>(DW_LNS_ADVANCE_LINE);
        w->WriteSLEB128(line_diff);
        w->Write<uint8_t>(DW_LNS_COPY);
      }

      // Increment the pc and line operands.
      pc += pc_diff;
      line += line_diff;
    }
    // Advance the pc to the end of the routine, since the end sequence opcode
    // requires this.
    w->Write<uint8_t>(DW_LNS_ADVANCE_PC);
    w->WriteSLEB128(desc_->CodeSize() - pc);
    WriteExtendedOpcode(w, DW_LNE_END_SEQUENCE, 0);
    total_length.set(static_cast<uint32_t>(w->position() - start));
    return true;
  }

 private:
  void WriteExtendedOpcode(Writer* w,
                           DWARF2ExtendedOpcode op,
                           size_t operands_size) {
    w->Write<uint8_t>(0);
    w->WriteULEB128(operands_size + 1);
    w->Write<uint8_t>(op);
  }

  static int ComparePCInfo(const GDBJITLineInfo::PCInfo* a,
                           const GDBJITLineInfo::PCInfo* b) {
    if (a->pc_ == b->pc_) {
      if (a->is_statement_ != b->is_statement_) {
        return b->is_statement_ ? +1 : -1;
      }
      return 0;
    } else if (a->pc_ > b->pc_) {
      return +1;
    } else {
      return -1;
    }
  }

  CodeDescription* desc_;
};


#ifdef V8_TARGET_ARCH_X64


class UnwindInfoSection : public ELFSection {
 public:
  explicit UnwindInfoSection(CodeDescription *desc);
  virtual bool WriteBody(Writer *w);

  int WriteCIE(Writer *w);
  void WriteFDE(Writer *w, int);

  void WriteFDEStateOnEntry(Writer *w);
  void WriteFDEStateAfterRBPPush(Writer *w);
  void WriteFDEStateAfterRBPSet(Writer *w);
  void WriteFDEStateAfterRBPPop(Writer *w);

  void WriteLength(Writer *w,
                   Writer::Slot<uint32_t>* length_slot,
                   int initial_position);

 private:
  CodeDescription *desc_;

  // DWARF3 Specification, Table 7.23
  enum CFIInstructions {
    DW_CFA_ADVANCE_LOC = 0x40,
    DW_CFA_OFFSET = 0x80,
    DW_CFA_RESTORE = 0xC0,
    DW_CFA_NOP = 0x00,
    DW_CFA_SET_LOC = 0x01,
    DW_CFA_ADVANCE_LOC1 = 0x02,
    DW_CFA_ADVANCE_LOC2 = 0x03,
    DW_CFA_ADVANCE_LOC4 = 0x04,
    DW_CFA_OFFSET_EXTENDED = 0x05,
    DW_CFA_RESTORE_EXTENDED = 0x06,
    DW_CFA_UNDEFINED = 0x07,
    DW_CFA_SAME_VALUE = 0x08,
    DW_CFA_REGISTER = 0x09,
    DW_CFA_REMEMBER_STATE = 0x0A,
    DW_CFA_RESTORE_STATE = 0x0B,
    DW_CFA_DEF_CFA = 0x0C,
    DW_CFA_DEF_CFA_REGISTER = 0x0D,
    DW_CFA_DEF_CFA_OFFSET = 0x0E,

    DW_CFA_DEF_CFA_EXPRESSION = 0x0F,
    DW_CFA_EXPRESSION = 0x10,
    DW_CFA_OFFSET_EXTENDED_SF = 0x11,
    DW_CFA_DEF_CFA_SF = 0x12,
    DW_CFA_DEF_CFA_OFFSET_SF = 0x13,
    DW_CFA_VAL_OFFSET = 0x14,
    DW_CFA_VAL_OFFSET_SF = 0x15,
    DW_CFA_VAL_EXPRESSION = 0x16
  };

  // System V ABI, AMD64 Supplement, Version 0.99.5, Figure 3.36
  enum RegisterMapping {
    // Only the relevant ones have been added to reduce clutter.
    AMD64_RBP = 6,
    AMD64_RSP = 7,
    AMD64_RA = 16
  };

  enum CFIConstants {
    CIE_ID = 0,
    CIE_VERSION = 1,
    CODE_ALIGN_FACTOR = 1,
    DATA_ALIGN_FACTOR = 1,
    RETURN_ADDRESS_REGISTER = AMD64_RA
  };
};


void UnwindInfoSection::WriteLength(Writer *w,
                                    Writer::Slot<uint32_t>* length_slot,
                                    int initial_position) {
  uint32_t align = (w->position() - initial_position) % kPointerSize;

  if (align != 0) {
    for (uint32_t i = 0; i < (kPointerSize - align); i++) {
      w->Write<uint8_t>(DW_CFA_NOP);
    }
  }

  ASSERT((w->position() - initial_position) % kPointerSize == 0);
  length_slot->set(w->position() - initial_position);
}


UnwindInfoSection::UnwindInfoSection(CodeDescription *desc)
    : ELFSection(".eh_frame", TYPE_X86_64_UNWIND, 1), desc_(desc)
{ }

int UnwindInfoSection::WriteCIE(Writer *w) {
  Writer::Slot<uint32_t> cie_length_slot = w->CreateSlotHere<uint32_t>();
  uint32_t cie_position = w->position();

  // Write out the CIE header. Currently no 'common instructions' are
  // emitted onto the CIE; every FDE has its own set of instructions.

  w->Write<uint32_t>(CIE_ID);
  w->Write<uint8_t>(CIE_VERSION);
  w->Write<uint8_t>(0);  // Null augmentation string.
  w->WriteSLEB128(CODE_ALIGN_FACTOR);
  w->WriteSLEB128(DATA_ALIGN_FACTOR);
  w->Write<uint8_t>(RETURN_ADDRESS_REGISTER);

  WriteLength(w, &cie_length_slot, cie_position);

  return cie_position;
}


void UnwindInfoSection::WriteFDE(Writer *w, int cie_position) {
  // The only FDE for this function. The CFA is the current RBP.
  Writer::Slot<uint32_t> fde_length_slot = w->CreateSlotHere<uint32_t>();
  int fde_position = w->position();
  w->Write<int32_t>(fde_position - cie_position + 4);

  w->Write<uintptr_t>(desc_->CodeStart());
  w->Write<uintptr_t>(desc_->CodeSize());

  WriteFDEStateOnEntry(w);
  WriteFDEStateAfterRBPPush(w);
  WriteFDEStateAfterRBPSet(w);
  WriteFDEStateAfterRBPPop(w);

  WriteLength(w, &fde_length_slot, fde_position);
}


void UnwindInfoSection::WriteFDEStateOnEntry(Writer *w) {
  // The first state, just after the control has been transferred to the the
  // function.

  // RBP for this function will be the value of RSP after pushing the RBP
  // for the previous function. The previous RBP has not been pushed yet.
  w->Write<uint8_t>(DW_CFA_DEF_CFA_SF);
  w->WriteULEB128(AMD64_RSP);
  w->WriteSLEB128(-kPointerSize);

  // The RA is stored at location CFA + kCallerPCOffset. This is an invariant,
  // and hence omitted from the next states.
  w->Write<uint8_t>(DW_CFA_OFFSET_EXTENDED);
  w->WriteULEB128(AMD64_RA);
  w->WriteSLEB128(StandardFrameConstants::kCallerPCOffset);

  // The RBP of the previous function is still in RBP.
  w->Write<uint8_t>(DW_CFA_SAME_VALUE);
  w->WriteULEB128(AMD64_RBP);

  // Last location described by this entry.
  w->Write<uint8_t>(DW_CFA_SET_LOC);
  w->Write<uint64_t>(
      desc_->GetStackStateStartAddress(CodeDescription::POST_RBP_PUSH));
}


void UnwindInfoSection::WriteFDEStateAfterRBPPush(Writer *w) {
  // The second state, just after RBP has been pushed.

  // RBP / CFA for this function is now the current RSP, so just set the
  // offset from the previous rule (from -8) to 0.
  w->Write<uint8_t>(DW_CFA_DEF_CFA_OFFSET);
  w->WriteULEB128(0);

  // The previous RBP is stored at CFA + kCallerFPOffset. This is an invariant
  // in this and the next state, and hence omitted in the next state.
  w->Write<uint8_t>(DW_CFA_OFFSET_EXTENDED);
  w->WriteULEB128(AMD64_RBP);
  w->WriteSLEB128(StandardFrameConstants::kCallerFPOffset);

  // Last location described by this entry.
  w->Write<uint8_t>(DW_CFA_SET_LOC);
  w->Write<uint64_t>(
      desc_->GetStackStateStartAddress(CodeDescription::POST_RBP_SET));
}


void UnwindInfoSection::WriteFDEStateAfterRBPSet(Writer *w) {
  // The third state, after the RBP has been set.

  // The CFA can now directly be set to RBP.
  w->Write<uint8_t>(DW_CFA_DEF_CFA);
  w->WriteULEB128(AMD64_RBP);
  w->WriteULEB128(0);

  // Last location described by this entry.
  w->Write<uint8_t>(DW_CFA_SET_LOC);
  w->Write<uint64_t>(
      desc_->GetStackStateStartAddress(CodeDescription::POST_RBP_POP));
}


void UnwindInfoSection::WriteFDEStateAfterRBPPop(Writer *w) {
  // The fourth (final) state. The RBP has been popped (just before issuing a
  // return).

  // The CFA can is now calculated in the same way as in the first state.
  w->Write<uint8_t>(DW_CFA_DEF_CFA_SF);
  w->WriteULEB128(AMD64_RSP);
  w->WriteSLEB128(-kPointerSize);

  // The RBP
  w->Write<uint8_t>(DW_CFA_OFFSET_EXTENDED);
  w->WriteULEB128(AMD64_RBP);
  w->WriteSLEB128(StandardFrameConstants::kCallerFPOffset);

  // Last location described by this entry.
  w->Write<uint8_t>(DW_CFA_SET_LOC);
  w->Write<uint64_t>(desc_->CodeEnd());
}


bool UnwindInfoSection::WriteBody(Writer *w) {
  uint32_t cie_position = WriteCIE(w);
  WriteFDE(w, cie_position);
  return true;
}


#endif  // V8_TARGET_ARCH_X64


static void CreateDWARFSections(CodeDescription* desc, ELF* elf) {
  if (desc->IsLineInfoAvailable()) {
    elf->AddSection(new DebugInfoSection(desc));
    elf->AddSection(new DebugAbbrevSection);
    elf->AddSection(new DebugLineSection(desc));
  }
#ifdef V8_TARGET_ARCH_X64
  elf->AddSection(new UnwindInfoSection(desc));
#endif
}


// -------------------------------------------------------------------
// Binary GDB JIT Interface as described in
//   http://sourceware.org/gdb/onlinedocs/gdb/Declarations.html
extern "C" {
  typedef enum {
    JIT_NOACTION = 0,
    JIT_REGISTER_FN,
    JIT_UNREGISTER_FN
  } JITAction;

  struct JITCodeEntry {
    JITCodeEntry* next_;
    JITCodeEntry* prev_;
    Address symfile_addr_;
    uint64_t symfile_size_;
  };

  struct JITDescriptor {
    uint32_t version_;
    uint32_t action_flag_;
    JITCodeEntry *relevant_entry_;
    JITCodeEntry *first_entry_;
  };

  // GDB will place breakpoint into this function.
  // To prevent GCC from inlining or removing it we place noinline attribute
  // and inline assembler statement inside.
  void __attribute__((noinline)) __jit_debug_register_code() {
    __asm__("");
  }

  // GDB will inspect contents of this descriptor.
  // Static initialization is necessary to prevent GDB from seeing
  // uninitialized descriptor.
  JITDescriptor __jit_debug_descriptor = { 1, 0, 0, 0 };
}


static JITCodeEntry* CreateCodeEntry(Address symfile_addr,
                                     uintptr_t symfile_size) {
  JITCodeEntry* entry = static_cast<JITCodeEntry*>(
      malloc(sizeof(JITCodeEntry) + symfile_size));

  entry->symfile_addr_ = reinterpret_cast<Address>(entry + 1);
  entry->symfile_size_ = symfile_size;
  memcpy(entry->symfile_addr_, symfile_addr, symfile_size);

  entry->prev_ = entry->next_ = NULL;

  return entry;
}


static void DestroyCodeEntry(JITCodeEntry* entry) {
  free(entry);
}


static void RegisterCodeEntry(JITCodeEntry* entry) {
#if defined(DEBUG) && !defined(WIN32)
  static int file_num = 0;
  if (FLAG_gdbjit_dump) {
    static const int kMaxFileNameSize = 64;
    static const char* kElfFilePrefix = "/tmp/elfdump";
    static const char* kObjFileExt = ".o";
    char file_name[64];

    OS::SNPrintF(Vector<char>(file_name, kMaxFileNameSize), "%s%d%s",
                 kElfFilePrefix, file_num++, kObjFileExt);
    WriteBytes(file_name, entry->symfile_addr_, entry->symfile_size_);
  }
#endif

  entry->next_ = __jit_debug_descriptor.first_entry_;
  if (entry->next_ != NULL) entry->next_->prev_ = entry;
  __jit_debug_descriptor.first_entry_ =
      __jit_debug_descriptor.relevant_entry_ = entry;

  __jit_debug_descriptor.action_flag_ = JIT_REGISTER_FN;
  __jit_debug_register_code();
}


static void UnregisterCodeEntry(JITCodeEntry* entry) {
  if (entry->prev_ != NULL) {
    entry->prev_->next_ = entry->next_;
  } else {
    __jit_debug_descriptor.first_entry_ = entry->next_;
  }

  if (entry->next_ != NULL) {
    entry->next_->prev_ = entry->prev_;
  }

  __jit_debug_descriptor.relevant_entry_ = entry;
  __jit_debug_descriptor.action_flag_ = JIT_UNREGISTER_FN;
  __jit_debug_register_code();
}


static JITCodeEntry* CreateELFObject(CodeDescription* desc) {
  ZoneScope zone_scope(DELETE_ON_EXIT);

  ELF elf;
  Writer w(&elf);

  int text_section_index = elf.AddSection(
      new FullHeaderELFSection(".text",
                               ELFSection::TYPE_NOBITS,
                               kCodeAlignment,
                               desc->CodeStart(),
                               0,
                               desc->CodeSize(),
                               ELFSection::FLAG_ALLOC | ELFSection::FLAG_EXEC));

  CreateSymbolsTable(desc, &elf, text_section_index);

  CreateDWARFSections(desc, &elf);

  elf.Write(&w);

  return CreateCodeEntry(w.buffer(), w.position());
}


static bool SameCodeObjects(void* key1, void* key2) {
  return key1 == key2;
}


static HashMap* GetEntries() {
  static HashMap* entries = NULL;
  if (entries == NULL) {
    entries = new HashMap(&SameCodeObjects);
  }
  return entries;
}


static uint32_t HashForCodeObject(Code* code) {
  static const uintptr_t kGoldenRatio = 2654435761u;
  uintptr_t hash = reinterpret_cast<uintptr_t>(code->address());
  return static_cast<uint32_t>((hash >> kCodeAlignmentBits) * kGoldenRatio);
}


static const intptr_t kLineInfoTag = 0x1;


static bool IsLineInfoTagged(void* ptr) {
  return 0 != (reinterpret_cast<intptr_t>(ptr) & kLineInfoTag);
}


static void* TagLineInfo(GDBJITLineInfo* ptr) {
  return reinterpret_cast<void*>(
      reinterpret_cast<intptr_t>(ptr) | kLineInfoTag);
}


static GDBJITLineInfo* UntagLineInfo(void* ptr) {
  return reinterpret_cast<GDBJITLineInfo*>(
      reinterpret_cast<intptr_t>(ptr) & ~kLineInfoTag);
}


void GDBJITInterface::AddCode(Handle<String> name,
                              Handle<Script> script,
                              Handle<Code> code) {
  if (!FLAG_gdbjit) return;

  // Force initialization of line_ends array.
  GetScriptLineNumber(script, 0);

  if (!name.is_null()) {
    SmartPointer<char> name_cstring = name->ToCString(DISALLOW_NULLS);
    AddCode(*name_cstring, *code, GDBJITInterface::FUNCTION, *script);
  } else {
    AddCode("", *code, GDBJITInterface::FUNCTION, *script);
  }
}

static void AddUnwindInfo(CodeDescription *desc) {
#ifdef V8_TARGET_ARCH_X64
  if (desc->tag() == GDBJITInterface::FUNCTION) {
    // To avoid propagating unwinding information through
    // compilation pipeline we use an approximation.
    // For most use cases this should not affect usability.
    static const int kFramePointerPushOffset = 1;
    static const int kFramePointerSetOffset = 4;
    static const int kFramePointerPopOffset = -3;

    uintptr_t frame_pointer_push_address =
        desc->CodeStart() + kFramePointerPushOffset;

    uintptr_t frame_pointer_set_address =
        desc->CodeStart() + kFramePointerSetOffset;

    uintptr_t frame_pointer_pop_address =
        desc->CodeEnd() + kFramePointerPopOffset;

    desc->SetStackStateStartAddress(CodeDescription::POST_RBP_PUSH,
                                    frame_pointer_push_address);
    desc->SetStackStateStartAddress(CodeDescription::POST_RBP_SET,
                                    frame_pointer_set_address);
    desc->SetStackStateStartAddress(CodeDescription::POST_RBP_POP,
                                    frame_pointer_pop_address);
  } else {
    desc->SetStackStateStartAddress(CodeDescription::POST_RBP_PUSH,
                                    desc->CodeStart());
    desc->SetStackStateStartAddress(CodeDescription::POST_RBP_SET,
                                    desc->CodeStart());
    desc->SetStackStateStartAddress(CodeDescription::POST_RBP_POP,
                                    desc->CodeEnd());
  }
#endif  // V8_TARGET_ARCH_X64
}


void GDBJITInterface::AddCode(const char* name,
                              Code* code,
                              GDBJITInterface::CodeTag tag,
                              Script* script) {
  if (!FLAG_gdbjit) return;
  AssertNoAllocation no_gc;

  HashMap::Entry* e = GetEntries()->Lookup(code, HashForCodeObject(code), true);
  if (e->value != NULL && !IsLineInfoTagged(e->value)) return;

  GDBJITLineInfo* lineinfo = UntagLineInfo(e->value);
  CodeDescription code_desc(name,
                            code,
                            script != NULL ? Handle<Script>(script)
                                           : Handle<Script>(),
                            lineinfo,
                            tag);

  if (!FLAG_gdbjit_full && !code_desc.IsLineInfoAvailable()) {
    delete lineinfo;
    GetEntries()->Remove(code, HashForCodeObject(code));
    return;
  }

  AddUnwindInfo(&code_desc);
  JITCodeEntry* entry = CreateELFObject(&code_desc);
  ASSERT(!IsLineInfoTagged(entry));

  delete lineinfo;
  e->value = entry;

  RegisterCodeEntry(entry);
}


void GDBJITInterface::AddCode(GDBJITInterface::CodeTag tag,
                              const char* name,
                              Code* code) {
  if (!FLAG_gdbjit) return;

  EmbeddedVector<char, 256> buffer;
  StringBuilder builder(buffer.start(), buffer.length());

  builder.AddString(Tag2String(tag));
  if ((name != NULL) && (*name != '\0')) {
    builder.AddString(": ");
    builder.AddString(name);
  } else {
    builder.AddFormatted(": code object %p", static_cast<void*>(code));
  }

  AddCode(builder.Finalize(), code, tag);
}


void GDBJITInterface::AddCode(GDBJITInterface::CodeTag tag,
                              String* name,
                              Code* code) {
  if (!FLAG_gdbjit) return;
  AddCode(tag, name != NULL ? *name->ToCString(DISALLOW_NULLS) : NULL, code);
}


void GDBJITInterface::AddCode(GDBJITInterface::CodeTag tag, Code* code) {
  if (!FLAG_gdbjit) return;

  AddCode(tag, "", code);
}


void GDBJITInterface::RemoveCode(Code* code) {
  if (!FLAG_gdbjit) return;

  HashMap::Entry* e = GetEntries()->Lookup(code,
                                           HashForCodeObject(code),
                                           false);
  if (e == NULL) return;

  if (IsLineInfoTagged(e->value)) {
    delete UntagLineInfo(e->value);
  } else {
    JITCodeEntry* entry = static_cast<JITCodeEntry*>(e->value);
    UnregisterCodeEntry(entry);
    DestroyCodeEntry(entry);
  }
  e->value = NULL;
  GetEntries()->Remove(code, HashForCodeObject(code));
}


void GDBJITInterface::RegisterDetailedLineInfo(Code* code,
                                               GDBJITLineInfo* line_info) {
  ASSERT(!IsLineInfoTagged(line_info));
  HashMap::Entry* e = GetEntries()->Lookup(code, HashForCodeObject(code), true);
  ASSERT(e->value == NULL);
  e->value = TagLineInfo(line_info);
}


} }  // namespace v8::internal
#endif
