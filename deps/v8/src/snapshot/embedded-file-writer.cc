// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded-file-writer.h"

#include <algorithm>
#include <cinttypes>

#include "src/objects/code-inl.h"

namespace v8 {
namespace internal {

// V8_CC_MSVC is true for both MSVC and clang on windows. clang can handle
// __asm__-style inline assembly but MSVC cannot, and thus we need a more
// precise compiler detection that can distinguish between the two. clang on
// windows sets both __clang__ and _MSC_VER, MSVC sets only _MSC_VER.
#if defined(_MSC_VER) && !defined(__clang__)
#define V8_COMPILER_IS_MSVC
#endif

// MSVC uses MASM for x86 and x64, while it has a ARMASM for ARM32 and
// ARMASM64 for ARM64. Since ARMASM and ARMASM64 accept a slightly tweaked
// version of ARM assembly language, they are referred to together in Visual
// Studio project files as MARMASM.
//
// ARM assembly language docs:
// http://infocenter.arm.com/help/topic/com.arm.doc.dui0802b/index.html
// Microsoft ARM assembler and assembly language docs:
// https://docs.microsoft.com/en-us/cpp/assembler/arm/arm-assembler-reference
#if defined(V8_COMPILER_IS_MSVC)
#if defined(V8_TARGET_ARCH_ARM64) || defined(V8_TARGET_ARCH_ARM)
#define V8_ASSEMBLER_IS_MARMASM
#elif defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X64)
#define V8_ASSEMBLER_IS_MASM
#else
#error Unknown Windows assembler target architecture.
#endif
#endif

// Name mangling.
// Symbols are prefixed with an underscore on 32-bit architectures.
#if defined(V8_TARGET_OS_WIN) && !defined(V8_TARGET_ARCH_X64) && \
    !defined(V8_TARGET_ARCH_ARM64)
#define SYMBOL_PREFIX "_"
#else
#define SYMBOL_PREFIX ""
#endif

// Platform-independent bits.
// -----------------------------------------------------------------------------

namespace {

DataDirective PointerSizeDirective() {
  if (kSystemPointerSize == 8) {
    return kQuad;
  } else {
    CHECK_EQ(4, kSystemPointerSize);
    return kLong;
  }
}

}  // namespace

const char* DirectiveAsString(DataDirective directive) {
#if defined(V8_TARGET_OS_WIN) && defined(V8_ASSEMBLER_IS_MASM)
  switch (directive) {
    case kByte:
      return "BYTE";
    case kLong:
      return "DWORD";
    case kQuad:
      return "QWORD";
    default:
      UNREACHABLE();
  }
#elif defined(V8_TARGET_OS_WIN) && defined(V8_ASSEMBLER_IS_MARMASM)
  switch (directive) {
    case kByte:
      return "DCB";
    case kLong:
      return "DCDU";
    case kQuad:
      return "DCQU";
    default:
      UNREACHABLE();
  }
#elif defined(V8_OS_AIX)
  switch (directive) {
    case kByte:
      return ".byte";
    case kLong:
      return ".long";
    case kQuad:
      return ".llong";
    default:
      UNREACHABLE();
  }
#else
  switch (directive) {
    case kByte:
      return ".byte";
    case kLong:
      return ".long";
    case kQuad:
      return ".quad";
    case kOcta:
      return ".octa";
  }
  UNREACHABLE();
#endif
}

void EmbeddedFileWriter::PrepareBuiltinSourcePositionMap(Builtins* builtins) {
  for (int i = 0; i < Builtins::builtin_count; i++) {
    // Retrieve the SourcePositionTable and copy it.
    Code code = builtins->builtin(i);
    // Verify that the code object is still the "real code" and not a
    // trampoline (which wouldn't have source positions).
    DCHECK(!code->is_off_heap_trampoline());
    std::vector<unsigned char> data(
        code->SourcePositionTable()->GetDataStartAddress(),
        code->SourcePositionTable()->GetDataEndAddress());
    source_positions_[i] = data;
  }
}

#if defined(V8_OS_WIN_X64)
std::string EmbeddedFileWriter::BuiltinsUnwindInfoLabel() const {
  char embedded_blob_data_symbol[kTemporaryStringLength];
  i::SNPrintF(i::Vector<char>(embedded_blob_data_symbol),
              "%s_Builtins_UnwindInfo", embedded_variant_);
  return embedded_blob_data_symbol;
}

void EmbeddedFileWriter::SetBuiltinUnwindData(
    int builtin_index, const win64_unwindinfo::BuiltinUnwindInfo& unwind_info) {
  DCHECK_LT(builtin_index, Builtins::builtin_count);
  unwind_infos_[builtin_index] = unwind_info;
}

void EmbeddedFileWriter::WriteUnwindInfoEntry(
    PlatformDependentEmbeddedFileWriter* w, uint64_t rva_start,
    uint64_t rva_end) const {
  w->DeclareRvaToSymbol(EmbeddedBlobDataSymbol().c_str(), rva_start);
  w->DeclareRvaToSymbol(EmbeddedBlobDataSymbol().c_str(), rva_end);
  w->DeclareRvaToSymbol(BuiltinsUnwindInfoLabel().c_str());
}

void EmbeddedFileWriter::WriteUnwindInfo(PlatformDependentEmbeddedFileWriter* w,
                                         const i::EmbeddedData* blob) const {
  // Emit an UNWIND_INFO (XDATA) struct, which contains the unwinding
  // information that is used for all builtin functions.
  DCHECK(win64_unwindinfo::CanEmitUnwindInfoForBuiltins());
  w->Comment("xdata for all the code in the embedded blob.");
  w->DeclareExternalFunction(CRASH_HANDLER_FUNCTION_NAME_STRING);

  w->StartXdataSection();
  {
    w->DeclareLabel(BuiltinsUnwindInfoLabel().c_str());
    std::vector<uint8_t> xdata =
        win64_unwindinfo::GetUnwindInfoForBuiltinFunctions();
    WriteBinaryContentsAsInlineAssembly(w, xdata.data(),
                                        static_cast<uint32_t>(xdata.size()));
    w->Comment("    ExceptionHandler");
    w->DeclareRvaToSymbol(CRASH_HANDLER_FUNCTION_NAME_STRING);
  }
  w->EndXdataSection();
  w->Newline();

  // Emit a RUNTIME_FUNCTION (PDATA) entry for each builtin function, as
  // documented here:
  // https://docs.microsoft.com/en-us/cpp/build/exception-handling-x64.
  w->Comment(
      "pdata for all the code in the embedded blob (structs of type "
      "RUNTIME_FUNCTION).");
  w->Comment("    BeginAddress");
  w->Comment("    EndAddress");
  w->Comment("    UnwindInfoAddress");
  w->StartPdataSection();
  {
    Address prev_builtin_end_offset = 0;
    for (int i = 0; i < Builtins::builtin_count; i++) {
      // Some builtins are leaf functions from the point of view of Win64 stack
      // walking: they do not move the stack pointer and do not require a PDATA
      // entry because the return address can be retrieved from [rsp].
      if (!blob->ContainsBuiltin(i)) continue;
      if (unwind_infos_[i].is_leaf_function()) continue;

      uint64_t builtin_start_offset = blob->InstructionStartOfBuiltin(i) -
                                      reinterpret_cast<Address>(blob->data());
      uint32_t builtin_size = blob->InstructionSizeOfBuiltin(i);

      const std::vector<int>& xdata_desc = unwind_infos_[i].fp_offsets();
      if (xdata_desc.empty()) {
        // Some builtins do not have any "push rbp - mov rbp, rsp" instructions
        // to start a stack frame. We still emit a PDATA entry as if they had,
        // relying on the fact that we can find the previous frame address from
        // rbp in most cases. Note that since the function does not really start
        // with a 'push rbp' we need to specify the start RVA in the PDATA entry
        // a few bytes before the beginning of the function, if it does not
        // overlap the end of the previous builtin.
        WriteUnwindInfoEntry(
            w,
            std::max(prev_builtin_end_offset,
                     builtin_start_offset - win64_unwindinfo::kRbpPrefixLength),
            builtin_start_offset + builtin_size);
      } else {
        // Some builtins have one or more "push rbp - mov rbp, rsp" sequences,
        // but not necessarily at the beginning of the function. In this case
        // we want to yield a PDATA entry for each block of instructions that
        // emit an rbp frame. If the function does not start with 'push rbp'
        // we also emit a PDATA entry for the initial block of code up to the
        // first 'push rbp', like in the case above.
        if (xdata_desc[0] > 0) {
          WriteUnwindInfoEntry(w,
                               std::max(prev_builtin_end_offset,
                                        builtin_start_offset -
                                            win64_unwindinfo::kRbpPrefixLength),
                               builtin_start_offset + xdata_desc[0]);
        }

        for (size_t j = 0; j < xdata_desc.size(); j++) {
          int chunk_start = xdata_desc[j];
          int chunk_end =
              (j < xdata_desc.size() - 1) ? xdata_desc[j + 1] : builtin_size;
          WriteUnwindInfoEntry(w, builtin_start_offset + chunk_start,
                               builtin_start_offset + chunk_end);
        }
      }

      prev_builtin_end_offset = builtin_start_offset + builtin_size;
      w->Newline();
    }
  }
  w->EndPdataSection();
  w->Newline();
}
#endif

// V8_OS_MACOSX
// Fuchsia target is explicitly excluded here for Mac hosts. This is to avoid
// generating uncompilable assembly files for the Fuchsia target.
// -----------------------------------------------------------------------------

#if defined(V8_OS_MACOSX) && !defined(V8_TARGET_OS_FUCHSIA)

void PlatformDependentEmbeddedFileWriter::SectionText() {
  fprintf(fp_, ".text\n");
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, ".data\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, ".const_data\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d", value);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  fprintf(fp_, "  %s _%s\n", DirectiveAsString(PointerSizeDirective()), target);
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  // TODO(jgruber): Investigate switching to .globl. Using .private_extern
  // prevents something along the compilation chain from messing with the
  // embedded blob. Using .global here causes embedded blob hash verification
  // failures at runtime.
  fprintf(fp_, ".private_extern _%s\n", name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {
  fprintf(fp_, ".balign 8\n");
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "_%s:\n", name);
}

void PlatformDependentEmbeddedFileWriter::SourceInfo(int fileid,
                                                     const char* filename,
                                                     int line) {
  fprintf(fp_, ".loc %d %d\n", fileid, line);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  DeclareLabel(name);

  // TODO(mvstanton): Investigate the proper incantations to mark the label as
  // a function on OSX.
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {}

void PlatformDependentEmbeddedFileWriter::DeclareExternalFilename(
    int fileid, const char* filename) {
  fprintf(fp_, ".file %d \"%s\"\n", fileid, filename);
}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

// V8_OS_AIX
// -----------------------------------------------------------------------------

#elif defined(V8_OS_AIX)

void PlatformDependentEmbeddedFileWriter::SectionText() {
  fprintf(fp_, ".csect .text[PR]\n");
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, ".csect .data[RW]\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, ".csect[RO]\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, ".align 2\n");
  fprintf(fp_, "%s:\n", name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d\n", value);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  AlignToCodeAlignment();
  DeclareLabel(name);
  fprintf(fp_, "  %s %s\n", DirectiveAsString(PointerSizeDirective()), target);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, ".globl %s\n", name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, ".align 5\n");
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {
  fprintf(fp_, ".align 3\n");
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s:\n", name);
}

void PlatformDependentEmbeddedFileWriter::SourceInfo(int fileid,
                                                     const char* filename,
                                                     int line) {
  fprintf(fp_, ".xline %d, \"%s\"\n", line, filename);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  Newline();
  DeclareSymbolGlobal(name);
  fprintf(fp_, ".csect %s[DS]\n", name); // function descriptor
  fprintf(fp_, "%s:\n", name);
  fprintf(fp_, ".llong .%s, 0, 0\n", name);
  SectionText();
  fprintf(fp_, ".%s:\n", name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {}

void PlatformDependentEmbeddedFileWriter::DeclareExternalFilename(
    int fileid, const char* filename) {
  // File name cannot be declared with an identifier on AIX.
  // We use the SourceInfo method to emit debug info in
  //.xline <line-number> <file-name> format.
}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

// V8_TARGET_OS_WIN (MSVC)
// -----------------------------------------------------------------------------

#elif defined(V8_TARGET_OS_WIN) && defined(V8_ASSEMBLER_IS_MASM)

// For MSVC builds we emit assembly in MASM syntax.
// See https://docs.microsoft.com/en-us/cpp/assembler/masm/directives-reference.

void PlatformDependentEmbeddedFileWriter::SectionText() {
  fprintf(fp_, ".CODE\n");
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, ".DATA\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, ".CONST\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s%s %s %d\n", SYMBOL_PREFIX, name, DirectiveAsString(kLong),
          value);
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s%s %s %s%s\n", SYMBOL_PREFIX, name,
          DirectiveAsString(PointerSizeDirective()), SYMBOL_PREFIX, target);
}

#if defined(V8_OS_WIN_X64)

void PlatformDependentEmbeddedFileWriter::StartPdataSection() {
  fprintf(fp_, "OPTION DOTNAME\n");
  fprintf(fp_, ".pdata SEGMENT DWORD READ ''\n");
}

void PlatformDependentEmbeddedFileWriter::EndPdataSection() {
  fprintf(fp_, ".pdata ENDS\n");
}

void PlatformDependentEmbeddedFileWriter::StartXdataSection() {
  fprintf(fp_, "OPTION DOTNAME\n");
  fprintf(fp_, ".xdata SEGMENT DWORD READ ''\n");
}

void PlatformDependentEmbeddedFileWriter::EndXdataSection() {
  fprintf(fp_, ".xdata ENDS\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareExternalFunction(
    const char* name) {
  fprintf(fp_, "EXTERN %s : PROC\n", name);
}

void PlatformDependentEmbeddedFileWriter::DeclareRvaToSymbol(const char* name,
                                                             uint64_t offset) {
  if (offset > 0) {
    fprintf(fp_, "DD IMAGEREL %s+%llu\n", name, offset);
  } else {
    fprintf(fp_, "DD IMAGEREL %s\n", name);
  }
}

#endif  // defined(V8_OS_WIN_X64)

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, "PUBLIC %s%s\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  // Diverges from other platforms due to compile error
  // 'invalid combination with segment alignment'.
  fprintf(fp_, "ALIGN 4\n");
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {
  fprintf(fp_, "ALIGN 4\n");
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "; %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "%s%s LABEL %s\n", SYMBOL_PREFIX, name,
          DirectiveAsString(kByte));
}

void PlatformDependentEmbeddedFileWriter::SourceInfo(int fileid,
                                                     const char* filename,
                                                     int line) {
  // TODO(mvstanton): output source information for MSVC.
  // Its syntax is #line <line> "<filename>"
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  fprintf(fp_, "%s%s PROC\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
  fprintf(fp_, "%s%s ENDP\n", SYMBOL_PREFIX, name);
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0%" PRIx64 "h", value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {
#if !defined(V8_TARGET_ARCH_X64)
  fprintf(fp_, ".MODEL FLAT\n");
#endif
}

void PlatformDependentEmbeddedFileWriter::DeclareExternalFilename(
    int fileid, const char* filename) {}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {
  fprintf(fp_, "END\n");
}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

#undef V8_ASSEMBLER_IS_MASM

#elif defined(V8_TARGET_OS_WIN) && defined(V8_ASSEMBLER_IS_MARMASM)

// The the AARCH64 ABI requires instructions be 4-byte-aligned and Windows does
// not have a stricter alignment requirement (see the TEXTAREA macro of
// kxarm64.h in the Windows SDK), so code is 4-byte-aligned.
// The data fields in the emitted assembly tend to be accessed with 8-byte
// LDR instructions, so data is 8-byte-aligned.
//
// armasm64's warning A4228 states
//     Alignment value exceeds AREA alignment; alignment not guaranteed
// To ensure that ALIGN directives are honored, their values are defined as
// equal to their corresponding AREA's ALIGN attributes.

#define ARM64_DATA_ALIGNMENT_POWER (3)
#define ARM64_DATA_ALIGNMENT (1 << ARM64_DATA_ALIGNMENT_POWER)
#define ARM64_CODE_ALIGNMENT_POWER (2)
#define ARM64_CODE_ALIGNMENT (1 << ARM64_CODE_ALIGNMENT_POWER)

void PlatformDependentEmbeddedFileWriter::SectionText() {
  fprintf(fp_, "  AREA |.text|, CODE, ALIGN=%d, READONLY\n",
          ARM64_CODE_ALIGNMENT_POWER);
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, "  AREA |.data|, DATA, ALIGN=%d, READWRITE\n",
          ARM64_DATA_ALIGNMENT_POWER);
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, "  AREA |.rodata|, DATA, ALIGN=%d, READONLY\n",
          ARM64_DATA_ALIGNMENT_POWER);
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s%s %s %d\n", SYMBOL_PREFIX, name, DirectiveAsString(kLong),
          value);
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s%s %s %s%s\n", SYMBOL_PREFIX, name,
          DirectiveAsString(PointerSizeDirective()), SYMBOL_PREFIX, target);
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, "  EXPORT %s%s\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, "  ALIGN %d\n", ARM64_CODE_ALIGNMENT);
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {
  fprintf(fp_, "  ALIGN %d\n", ARM64_DATA_ALIGNMENT);
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "; %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "%s%s\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::SourceInfo(int fileid,
                                                     const char* filename,
                                                     int line) {
  // TODO(mvstanton): output source information for MSVC.
  // Its syntax is #line <line> "<filename>"
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  fprintf(fp_, "%s%s FUNCTION\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
  fprintf(fp_, "  ENDFUNC\n");
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {}

void PlatformDependentEmbeddedFileWriter::DeclareExternalFilename(
    int fileid, const char* filename) {}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {
  fprintf(fp_, "  END\n");
}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

#undef V8_ASSEMBLER_IS_MARMASM
#undef ARM64_DATA_ALIGNMENT_POWER
#undef ARM64_DATA_ALIGNMENT
#undef ARM64_CODE_ALIGNMENT_POWER
#undef ARM64_CODE_ALIGNMENT

// Everything but AIX, Windows with MSVC, or OSX.
// -----------------------------------------------------------------------------

#else

void PlatformDependentEmbeddedFileWriter::SectionText() {
#ifdef OS_CHROMEOS
  fprintf(fp_, ".section .text.hot.embedded\n");
#else
  fprintf(fp_, ".section .text\n");
#endif
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, ".section .data\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  if (i::FLAG_target_os == std::string("win"))
    fprintf(fp_, ".section .rdata\n");
  else
    fprintf(fp_, ".section .rodata\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d", value);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  fprintf(fp_, "  %s %s%s\n", DirectiveAsString(PointerSizeDirective()),
          SYMBOL_PREFIX, target);
}

#if defined(V8_OS_WIN_X64)

void PlatformDependentEmbeddedFileWriter::StartPdataSection() {
  fprintf(fp_, ".section .pdata\n");
}

void PlatformDependentEmbeddedFileWriter::EndPdataSection() {}

void PlatformDependentEmbeddedFileWriter::StartXdataSection() {
  fprintf(fp_, ".section .xdata\n");
}

void PlatformDependentEmbeddedFileWriter::EndXdataSection() {}

void PlatformDependentEmbeddedFileWriter::DeclareExternalFunction(
    const char* name) {}

void PlatformDependentEmbeddedFileWriter::DeclareRvaToSymbol(const char* name,
                                                             uint64_t offset) {
  if (offset > 0) {
    fprintf(fp_, ".rva %s + %llu\n", name, offset);
  } else {
    fprintf(fp_, ".rva %s\n", name);
  }
}

#endif  // defined(V8_OS_WIN_X64)

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, ".global %s%s\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {
  // On Windows ARM64, s390, PPC and possibly more platforms, aligned load
  // instructions are used to retrieve v8_Default_embedded_blob_ and/or
  // v8_Default_embedded_blob_size_. The generated instructions require the
  // load target to be aligned at 8 bytes (2^3).
  fprintf(fp_, ".balign 8\n");
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "%s%s:\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::SourceInfo(int fileid,
                                                     const char* filename,
                                                     int line) {
  fprintf(fp_, ".loc %d %d\n", fileid, line);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  DeclareLabel(name);

  if (i::FLAG_target_os == std::string("win")) {
#if defined(V8_TARGET_ARCH_ARM64)
    // Windows ARM64 assembly is in GAS syntax, but ".type" is invalid directive
    // in PE/COFF for Windows.
#else
    // The directives for inserting debugging information on Windows come
    // from the PE (Portable Executable) and COFF (Common Object File Format)
    // standards. Documented here:
    // https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format
    //
    // .scl 2 means StorageClass external.
    // .type 32 means Type Representation Function.
    fprintf(fp_, ".def %s%s; .scl 2; .type 32; .endef;\n", SYMBOL_PREFIX, name);
#endif
  } else {
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_ARM64)
    // ELF format binaries on ARM use ".type <function name>, %function"
    // to create a DWARF subprogram entry.
    fprintf(fp_, ".type %s, %%function\n", name);
#else
    // Other ELF Format binaries use ".type <function name>, @function"
    // to create a DWARF subprogram entry.
    fprintf(fp_, ".type %s, @function\n", name);
#endif
  }
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {}

void PlatformDependentEmbeddedFileWriter::DeclareExternalFilename(
    int fileid, const char* filename) {
  // Replace any Windows style paths (backslashes) with forward
  // slashes.
  std::string fixed_filename(filename);
  std::replace(fixed_filename.begin(), fixed_filename.end(), '\\', '/');
  fprintf(fp_, ".file %d \"%s\"\n", fileid, fixed_filename.c_str());
}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

#endif

#undef SYMBOL_PREFIX
#undef V8_COMPILER_IS_MSVC

}  // namespace internal
}  // namespace v8
