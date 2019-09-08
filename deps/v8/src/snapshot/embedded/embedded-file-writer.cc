// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/embedded-file-writer.h"

#include <cinttypes>

#include "src/codegen/source-position-table.h"
#include "src/objects/code-inl.h"

namespace v8 {
namespace internal {

void EmbeddedFileWriter::WriteBuiltin(PlatformEmbeddedFileWriterBase* w,
                                      const i::EmbeddedData* blob,
                                      const int builtin_id) const {
  const bool is_default_variant =
      std::strcmp(embedded_variant_, kDefaultEmbeddedVariant) == 0;

  i::EmbeddedVector<char, kTemporaryStringLength> builtin_symbol;
  if (is_default_variant) {
    // Create nicer symbol names for the default mode.
    i::SNPrintF(builtin_symbol, "Builtins_%s", i::Builtins::name(builtin_id));
  } else {
    i::SNPrintF(builtin_symbol, "%s_Builtins_%s", embedded_variant_,
                i::Builtins::name(builtin_id));
  }

  // Labels created here will show up in backtraces. We check in
  // Isolate::SetEmbeddedBlob that the blob layout remains unchanged, i.e.
  // that labels do not insert bytes into the middle of the blob byte
  // stream.
  w->DeclareFunctionBegin(builtin_symbol.begin());
  const std::vector<byte>& current_positions = source_positions_[builtin_id];

  // The code below interleaves bytes of assembly code for the builtin
  // function with source positions at the appropriate offsets.
  Vector<const byte> vpos(current_positions.data(), current_positions.size());
  v8::internal::SourcePositionTableIterator positions(
      vpos, SourcePositionTableIterator::kExternalOnly);

  const uint8_t* data = reinterpret_cast<const uint8_t*>(
      blob->InstructionStartOfBuiltin(builtin_id));
  uint32_t size = blob->PaddedInstructionSizeOfBuiltin(builtin_id);
  uint32_t i = 0;
  uint32_t next_offset =
      static_cast<uint32_t>(positions.done() ? size : positions.code_offset());
  while (i < size) {
    if (i == next_offset) {
      // Write source directive.
      w->SourceInfo(positions.source_position().ExternalFileId(),
                    GetExternallyCompiledFilename(
                        positions.source_position().ExternalFileId()),
                    positions.source_position().ExternalLine());
      positions.Advance();
      next_offset = static_cast<uint32_t>(
          positions.done() ? size : positions.code_offset());
    }
    CHECK_GE(next_offset, i);
    WriteBinaryContentsAsInlineAssembly(w, data + i, next_offset - i);
    i = next_offset;
  }

  w->DeclareFunctionEnd(builtin_symbol.begin());
}

void EmbeddedFileWriter::WriteFileEpilogue(PlatformEmbeddedFileWriterBase* w,
                                           const i::EmbeddedData* blob) const {
  {
    i::EmbeddedVector<char, kTemporaryStringLength> embedded_blob_symbol;
    i::SNPrintF(embedded_blob_symbol, "v8_%s_embedded_blob_",
                embedded_variant_);

    w->Comment("Pointer to the beginning of the embedded blob.");
    w->SectionData();
    w->AlignToDataAlignment();
    w->DeclarePointerToSymbol(embedded_blob_symbol.begin(),
                              EmbeddedBlobDataSymbol().c_str());
    w->Newline();
  }

  {
    i::EmbeddedVector<char, kTemporaryStringLength> embedded_blob_size_symbol;
    i::SNPrintF(embedded_blob_size_symbol, "v8_%s_embedded_blob_size_",
                embedded_variant_);

    w->Comment("The size of the embedded blob in bytes.");
    w->SectionRoData();
    w->AlignToDataAlignment();
    w->DeclareUint32(embedded_blob_size_symbol.begin(), blob->size());
    w->Newline();
  }

#if defined(V8_OS_WIN64)
  {
    i::EmbeddedVector<char, kTemporaryStringLength> unwind_info_symbol;
    i::SNPrintF(unwind_info_symbol, "%s_Builtins_UnwindInfo",
                embedded_variant_);

    w->MaybeEmitUnwindData(unwind_info_symbol.begin(),
                           EmbeddedBlobDataSymbol().c_str(), blob,
                           reinterpret_cast<const void*>(&unwind_infos_[0]));
  }
#endif  // V8_OS_WIN64

  w->FileEpilogue();
}

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

// static
void EmbeddedFileWriter::WriteBinaryContentsAsInlineAssembly(
    PlatformEmbeddedFileWriterBase* w, const uint8_t* data, uint32_t size) {
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
  for (int i = 0; i < Builtins::builtin_count; i++) {
    // Retrieve the SourcePositionTable and copy it.
    Code code = builtins->builtin(i);
    // Verify that the code object is still the "real code" and not a
    // trampoline (which wouldn't have source positions).
    DCHECK(!code.is_off_heap_trampoline());
    std::vector<unsigned char> data(
        code.SourcePositionTable().GetDataStartAddress(),
        code.SourcePositionTable().GetDataEndAddress());
    source_positions_[i] = data;
  }
}

}  // namespace internal
}  // namespace v8
