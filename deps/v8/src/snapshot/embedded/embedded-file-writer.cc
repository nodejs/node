// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/embedded-file-writer.h"

#include <algorithm>
#include <cinttypes>

#include "src/codegen/code-comments.h"
#include "src/codegen/source-position-table.h"
#include "src/flags/flags.h"  // For ENABLE_CONTROL_FLOW_INTEGRITY_BOOL
#include "src/objects/code-inl.h"
#include "src/snapshot/embedded/embedded-data-inl.h"

namespace v8 {
namespace internal {

namespace {

int WriteDirectiveOrSeparator(PlatformEmbeddedFileWriterBase* w,
                              int current_line_length,
                              DataDirective directive) {
  int printed_chars;
  if (current_line_length == 0) {
    printed_chars = w->IndentedDataDirective(directive);
    DCHECK_LT(0, printed_chars);
  } else {
    printed_chars = fprintf(w->fp(), ",");
    DCHECK_EQ(1, printed_chars);
  }
  return current_line_length + printed_chars;
}

int WriteLineEndIfNeeded(PlatformEmbeddedFileWriterBase* w,
                         int current_line_length, int write_size) {
  static const int kTextWidth = 100;
  // Check if adding ',0xFF...FF\n"' would force a line wrap. This doesn't use
  // the actual size of the string to be written to determine this so it's
  // more conservative than strictly needed.
  if (current_line_length + strlen(",0x") + write_size * 2 > kTextWidth) {
    fprintf(w->fp(), "\n");
    return 0;
  } else {
    return current_line_length;
  }
}

}  // namespace

void EmbeddedFileWriter::WriteBuiltin(PlatformEmbeddedFileWriterBase* w,
                                      const i::EmbeddedData* blob,
                                      const Builtin builtin) const {
  const bool is_default_variant =
      std::strcmp(embedded_variant_, kDefaultEmbeddedVariant) == 0;

  base::EmbeddedVector<char, kTemporaryStringLength> builtin_symbol;
  if (is_default_variant) {
    // Create nicer symbol names for the default mode.
    base::SNPrintF(builtin_symbol, "Builtins_%s", i::Builtins::name(builtin));
  } else {
    base::SNPrintF(builtin_symbol, "%s_Builtins_%s", embedded_variant_,
                   i::Builtins::name(builtin));
  }

  // Labels created here will show up in backtraces. We check in
  // Isolate::SetEmbeddedBlob that the blob layout remains unchanged, i.e.
  // that labels do not insert bytes into the middle of the blob byte
  // stream.
  w->DeclareFunctionBegin(builtin_symbol.begin(),
                          blob->InstructionSizeOf(builtin));

  if (builtin_marker_sources_id_ != 0) {
    // Depending on the kind of builtin we don't actually have line based source
    // information. To avoid gdb bleeding the line of the previous builtin into
    // this one we emit a marker <builtin> source line info.
    w->SourceInfo(builtin_marker_sources_id_,
                  GetExternallyCompiledFilename(builtin_marker_sources_id_), 1);
  }

  const int builtin_id = static_cast<int>(builtin);
  const std::vector<uint8_t>& current_comments = code_comments_[builtin_id];
  const std::vector<uint8_t>& current_positions = source_positions_[builtin_id];
  // The code below interleaves bytes of assembly code for the builtin
  // function with source positions at the appropriate offsets.
  base::Vector<const uint8_t> vpos(current_positions.data(),
                                   current_positions.size());
  v8::internal::SourcePositionTableIterator positions(
      vpos, SourcePositionTableIterator::kExternalOnly);

#ifndef DEBUG
  CHECK(positions.done());  // Release builds must not contain debug infos.
#endif

  // Some builtins (InterpreterPushArgsThenFastConstructFunction,
  // JSConstructStubGeneric) have entry points located in the middle of them, we
  // need to store their addresses since they are part of the list of allowed
  // return addresses in the deoptimizer.
  const std::vector<LabelInfo>& current_labels = label_info_[builtin_id];
  auto label = current_labels.begin();

  std::optional<CodeCommentsIterator> cmt_it;
  if (!current_comments.empty()) {
    cmt_it.emplace(reinterpret_cast<Address>(current_comments.data()),
                   static_cast<uint32_t>(current_comments.size()));
  }

  const uint8_t* data =
      reinterpret_cast<const uint8_t*>(blob->InstructionStartOf(builtin));
  uint32_t size = blob->PaddedInstructionSizeOf(builtin);
  uint32_t i = 0;
  uint32_t next_source_pos_offset =
      static_cast<uint32_t>(positions.done() ? size : positions.code_offset());
  uint32_t next_label_offset = static_cast<uint32_t>(
      (label == current_labels.end()) ? size : label->offset);
  uint32_t next_cmt_offset = static_cast<uint32_t>(
      (cmt_it && cmt_it->HasCurrent()) ? cmt_it->GetPCOffset() : size);
  uint32_t next_offset = 0;
  while (i < size) {
    if (i == next_source_pos_offset) {
      // Write source directive.
      w->SourceInfo(positions.source_position().ExternalFileId(),
                    GetExternallyCompiledFilename(
                        positions.source_position().ExternalFileId()),
                    positions.source_position().ExternalLine());
      positions.Advance();
      next_source_pos_offset = static_cast<uint32_t>(
          positions.done() ? size : positions.code_offset());
      CHECK_GE(next_source_pos_offset, i);
    }
    if (i == next_label_offset) {
      WriteBuiltinLabels(w, label->name);
      label++;
      next_label_offset = static_cast<uint32_t>(
          (label == current_labels.end()) ? size : label->offset);
      CHECK_GE(next_label_offset, i);
    }
    if (i == next_cmt_offset) {
      const char* comment = cmt_it->GetComment();
      if (strncmp(comment, "CFI:", 4) == 0) {
        std::string dir(comment + 4);
        size_t pos = dir.find(" - ");
        if (pos != std::string::npos) {
          dir = dir.substr(0, pos);
        }
        fprintf(w->fp(), "%s\n", dir.c_str());
      } else {
        w->Comment(comment);
      }
      cmt_it->Next();
      next_cmt_offset = static_cast<uint32_t>(
          (cmt_it && cmt_it->HasCurrent()) ? cmt_it->GetPCOffset() : size);
    }
    next_offset =
        std::min({next_source_pos_offset, next_label_offset, next_cmt_offset});
    WriteBinaryContentsAsInlineAssembly(w, data + i, next_offset - i);
    i = next_offset;
  }

  w->DeclareFunctionEnd(builtin_symbol.begin());
}

void EmbeddedFileWriter::WriteBuiltinLabels(PlatformEmbeddedFileWriterBase* w,
                                            std::string name) const {
  w->DeclareLabel(name.c_str());
}

void EmbeddedFileWriter::WriteCodeSection(PlatformEmbeddedFileWriterBase* w,
                                          const i::EmbeddedData* blob) const {
  if (v8_flags.torque_dwarf) {
    FILE* fp = w->fp();
    fprintf(fp, ".section .debug_line,\"\",@progbits\n");
    fprintf(fp, ".Lline_table_start0:\n");
  }

  w->Comment(
      "The embedded blob code section starts here. It contains the builtin");
  w->Comment("instruction streams.");
  w->SectionText();

#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
  // UMA needs an exposed function-type label at the start of the embedded
  // code section.
  static const char* kCodeStartForProfilerSymbolName =
      "v8_code_start_for_profiler_";
  static constexpr int kDummyFunctionLength = 1;
  static constexpr int kDummyFunctionData = 0xcc;
  w->DeclareFunctionBegin(kCodeStartForProfilerSymbolName,
                          kDummyFunctionLength);
  // The label must not be at the same address as the first builtin, insert
  // padding bytes.
  WriteDirectiveOrSeparator(w, 0, kByte);
  w->HexLiteral(kDummyFunctionData);
  w->Newline();
  w->DeclareFunctionEnd(kCodeStartForProfilerSymbolName);
#endif

  w->AlignToCodeAlignment();
  w->DeclareSymbolGlobal(EmbeddedBlobCodeSymbol().c_str());
  w->DeclareLabelProlog(EmbeddedBlobCodeSymbol().c_str());
  w->DeclareLabel(EmbeddedBlobCodeSymbol().c_str());

  static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
  for (ReorderedBuiltinIndex embedded_index = 0;
       embedded_index < Builtins::kBuiltinCount; embedded_index++) {
    Builtin builtin = blob->GetBuiltinId(embedded_index);
    WriteBuiltin(w, blob, builtin);
  }
  w->AlignToPageSizeIfNeeded();
  w->DeclareLabelEpilogue();
  w->Newline();
}

void EmbeddedFileWriter::WriteDebugSection(PlatformEmbeddedFileWriterBase* w,
                                           const i::EmbeddedData* blob) const {
  if (!v8_flags.torque_dwarf) return;

  FILE* fp = w->fp();

  // 1. .debug_abbrev
  fprintf(fp, ".section .debug_abbrev,\"\",@progbits\n");
  fprintf(fp, ".Ldebug_abbrev0:\n");
  fprintf(fp, "  .byte 1\n");   // Abbrev 1
  fprintf(fp, "  .byte 17\n");  // DW_TAG_compile_unit
  fprintf(fp, "  .byte 1\n");   // DW_CHILDREN_yes
  fprintf(fp, "  .byte 37\n");  // DW_AT_producer
  fprintf(fp, "  .byte 14\n");  // DW_FORM_strp
  fprintf(fp, "  .byte 19\n");  // DW_AT_language
  fprintf(fp, "  .byte 5\n");   // DW_FORM_data2
  fprintf(fp, "  .byte 3\n");   // DW_AT_name
  fprintf(fp, "  .byte 14\n");  // DW_FORM_strp
  fprintf(fp, "  .byte 16\n");  // DW_AT_stmt_list
  fprintf(fp, "  .byte 23\n");  // DW_FORM_sec_offset
  fprintf(fp, "  .byte 27\n");  // DW_AT_comp_dir
  fprintf(fp, "  .byte 14\n");  // DW_FORM_strp
  fprintf(fp, "  .byte 17\n");  // DW_AT_low_pc
  fprintf(fp, "  .byte 1\n");   // DW_FORM_addr
  fprintf(fp, "  .byte 18\n");  // DW_AT_high_pc
  fprintf(fp, "  .byte 6\n");   // DW_FORM_data4
  fprintf(fp, "  .byte 0\n");   // EOM
  fprintf(fp, "  .byte 0\n");   // EOM

  fprintf(fp, "  .byte 2\n");   // Abbrev 2
  fprintf(fp, "  .byte 46\n");  // DW_TAG_subprogram
  fprintf(fp, "  .byte 0\n");   // DW_CHILDREN_no
  fprintf(fp, "  .byte 3\n");   // DW_AT_name
  fprintf(fp, "  .byte 14\n");  // DW_FORM_strp
  fprintf(fp, "  .byte 17\n");  // DW_AT_low_pc
  fprintf(fp, "  .byte 1\n");   // DW_FORM_addr
  fprintf(fp, "  .byte 18\n");  // DW_AT_high_pc
  fprintf(fp, "  .byte 6\n");   // DW_FORM_data4
  fprintf(fp, "  .byte 0\n");   // EOM
  fprintf(fp, "  .byte 0\n");   // EOM
  fprintf(fp, "  .byte 0\n");   // End of abbrev

  // 2. .debug_info
  fprintf(fp, ".section .debug_info,\"\",@progbits\n");
  fprintf(fp, ".Lcu_begin0:\n");
  fprintf(fp, "  .long .Ldebug_info_end0 - .Ldebug_info_start0\n");  // Length
  fprintf(fp, ".Ldebug_info_start0:\n");
  fprintf(fp, "  .short 4\n");                      // Version
  fprintf(fp, "  .long .debug_abbrev\n");           // Abbrev offset
  fprintf(fp, "  .byte %d\n", kSystemPointerSize);  // Address size

  fprintf(fp, "  .byte 1\n");                    // Abbrev 1 (Compile Unit)
  fprintf(fp, "  .long .Linfo_string0\n");       // Producer
  fprintf(fp, "  .short 2\n");                   // Language (C)
  fprintf(fp, "  .long .Linfo_string1\n");       // Name
  fprintf(fp, "  .long .Lline_table_start0\n");  // Stmt list
  fprintf(fp, "  .long .Linfo_string2\n");       // Comp dir
  w->IndentedDataDirective(PointerSizeDirective());
  fprintf(fp, "%s\n", EmbeddedBlobCodeSymbol().c_str());  // Low PC
  fprintf(fp, "  .long %u\n", blob->code_size());         // High PC

  int string_offset = 3;  // We already have 3 strings in header!

  for (ReorderedBuiltinIndex embedded_index = 0;
       embedded_index < Builtins::kBuiltinCount; embedded_index++) {
    Builtin builtin = blob->GetBuiltinId(embedded_index);
    const char* name = Builtins::name(builtin);

    fprintf(fp, "  .byte 2\n");  // Abbrev 2 (Subprogram)
    fprintf(fp, "  .long .Linfo_string%d\n", string_offset++);  // Name
    w->IndentedDataDirective(PointerSizeDirective());
    fprintf(fp, "Builtins_%s\n", name);                             // Low PC
    fprintf(fp, "  .long %u\n", blob->InstructionSizeOf(builtin));  // High PC
  }
  fprintf(fp, "  .byte 0\n");  // End of children
  fprintf(fp, ".Ldebug_info_end0:\n");

  // 3. .debug_str
  fprintf(fp, ".section .debug_str,\"MS\",@progbits,1\n");
  fprintf(fp, ".Linfo_string0:\n");
  fprintf(fp, "  .asciz \"Torque\"\n");
  fprintf(fp, ".Linfo_string1:\n");
  fprintf(fp, "  .asciz \"embedded.S\"\n");
  fprintf(fp, ".Linfo_string2:\n");
  fprintf(fp, "  .asciz \".\"\n");

  string_offset = 3;
  for (ReorderedBuiltinIndex embedded_index = 0;
       embedded_index < Builtins::kBuiltinCount; embedded_index++) {
    Builtin builtin = blob->GetBuiltinId(embedded_index);
    const char* name = Builtins::name(builtin);
    fprintf(fp, ".Linfo_string%d:\n", string_offset++);
    fprintf(fp, "  .asciz \"Builtins_%s\"\n", name);
  }
  fprintf(fp, "\n");
}

void EmbeddedFileWriter::WriteFileEpilogue(PlatformEmbeddedFileWriterBase* w,
                                           const i::EmbeddedData* blob) const {
  {
    base::EmbeddedVector<char, kTemporaryStringLength>
        embedded_blob_code_size_symbol;
    base::SNPrintF(embedded_blob_code_size_symbol,
                   "v8_%s_embedded_blob_code_size_", embedded_variant_);

    w->Comment("The size of the embedded blob code in bytes.");
    w->SectionRoData();
    w->AlignToDataAlignment();
    w->DeclareUint32(embedded_blob_code_size_symbol.begin(), blob->code_size());
    w->Newline();

    base::EmbeddedVector<char, kTemporaryStringLength>
        embedded_blob_data_size_symbol;
    base::SNPrintF(embedded_blob_data_size_symbol,
                   "v8_%s_embedded_blob_data_size_", embedded_variant_);

    w->Comment("The size of the embedded blob data section in bytes.");
    w->DeclareUint32(embedded_blob_data_size_symbol.begin(), blob->data_size());
    w->Newline();
  }

#if defined(V8_OS_WIN64)
  {
    base::EmbeddedVector<char, kTemporaryStringLength> unwind_info_symbol;
    base::SNPrintF(unwind_info_symbol, "%s_Builtins_UnwindInfo",
                   embedded_variant_);

    w->MaybeEmitUnwindData(unwind_info_symbol.begin(),
                           EmbeddedBlobCodeSymbol().c_str(), blob,
                           reinterpret_cast<const void*>(&unwind_infos_[0]));
  }
#endif  // V8_OS_WIN64

  w->FileEpilogue();
}

// static
void EmbeddedFileWriter::WriteBinaryContentsAsInlineAssembly(
    PlatformEmbeddedFileWriterBase* w, const uint8_t* data, uint32_t size) {
#if V8_OS_ZOS
  // HLASM source must end at column 71 (followed by an optional
  // line-continuation char on column 72), so write the binary data
  // in 32 byte chunks (length 64):
  uint32_t chunks = (size + 31) / 32;
  uint32_t i, j;
  uint32_t offset = 0;
  for (i = 0; i < chunks; ++i) {
    fprintf(w->fp(), " DC x'");
    for (j = 0; offset < size && j < 32; ++j) {
      fprintf(w->fp(), "%02x", data[offset++]);
    }
    fprintf(w->fp(), "'\n");
  }
#else
  int current_line_length = 0;
  uint32_t i = 0;

  // Begin by writing out byte chunks.
  const DataDirective directive = w->ByteChunkDataDirective();
  const int byte_chunk_size = DataDirectiveSize(directive);
  for (; i + byte_chunk_size < size; i += byte_chunk_size) {
    current_line_length =
        WriteDirectiveOrSeparator(w, current_line_length, directive);
    current_line_length += w->WriteByteChunk(data + i);
    current_line_length =
        WriteLineEndIfNeeded(w, current_line_length, byte_chunk_size);
  }
  if (current_line_length != 0) w->Newline();
  current_line_length = 0;

  // Write any trailing bytes one-by-one.
  for (; i < size; i++) {
    current_line_length =
        WriteDirectiveOrSeparator(w, current_line_length, kByte);
    current_line_length += w->HexLiteral(data[i]);
    current_line_length = WriteLineEndIfNeeded(w, current_line_length, 1);
  }

  if (current_line_length != 0) w->Newline();
#endif  // V8_OS_ZOS
}

int EmbeddedFileWriter::LookupOrAddExternallyCompiledFilename(
    const char* filename) {
  auto result = external_filenames_.find(filename);
  if (result != external_filenames_.end()) {
    return result->second;
  }
  int new_id =
      ExternalFilenameIndexToId(static_cast<int>(external_filenames_.size()));
  external_filenames_.insert(std::make_pair(filename, new_id));
  external_filenames_by_index_.push_back(filename);
  DCHECK_EQ(external_filenames_by_index_.size(), external_filenames_.size());
  return new_id;
}

const char* EmbeddedFileWriter::GetExternallyCompiledFilename(
    int fileid) const {
  size_t index = static_cast<size_t>(ExternalFilenameIdToIndex(fileid));
  DCHECK_GE(index, 0);
  DCHECK_LT(index, external_filenames_by_index_.size());

  return external_filenames_by_index_[index];
}

int EmbeddedFileWriter::GetExternallyCompiledFilenameCount() const {
  return static_cast<int>(external_filenames_.size());
}

void EmbeddedFileWriter::PrepareBuiltinSourcePositionMap(Builtins* builtins) {
  bool any_source_positions = false;
  for (Builtin builtin = Builtins::kFirst; builtin <= Builtins::kLast;
       ++builtin) {
    // Retrieve the SourcePositionTable and copy it.
    Tagged<Code> code = builtins->code(builtin);
    if (code->has_source_position_table()) {
      Tagged<TrustedByteArray> source_position_table =
          code->source_position_table();
      std::vector<unsigned char> data(source_position_table->begin(),
                                      source_position_table->end());
      if (!data.empty()) {
        any_source_positions = true;
      }
      source_positions_[static_cast<int>(builtin)] = data;
    }
    // Also copy code comments if present.
    if (code->code_comments_size() > 0) {
      std::vector<unsigned char> data(
          reinterpret_cast<unsigned char*>(code->code_comments()),
          reinterpret_cast<unsigned char*>(code->code_comments()) +
              code->code_comments_size());
      code_comments_[static_cast<int>(builtin)] = data;
    }
  }
  if (any_source_positions) {
    builtin_marker_sources_id_ =
        LookupOrAddExternallyCompiledFilename("<builtin>");
  }
}

}  // namespace internal
}  // namespace v8
