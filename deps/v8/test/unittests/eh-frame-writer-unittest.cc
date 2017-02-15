// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/eh-frame.h"
#include "test/unittests/test-utils.h"

// Test enabled only on supported architectures.
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM) || \
    defined(V8_TARGET_ARCH_ARM64)

using namespace v8::internal;

namespace {

class EhFrameWriterTest : public TestWithZone {
 protected:
  // Being a 7bit positive integer, this also serves as its ULEB128 encoding.
  static const int kTestRegisterCode = 0;

  static EhFrameIterator MakeIterator(EhFrameWriter* writer) {
    CodeDesc desc;
    writer->GetEhFrame(&desc);
    DCHECK_GT(desc.unwinding_info_size, 0);
    return EhFrameIterator(desc.unwinding_info,
                           desc.unwinding_info + desc.unwinding_info_size);
  }
};

const int EhFrameWriterTest::kTestRegisterCode;

}  // namespace

TEST_F(EhFrameWriterTest, Alignment) {
  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(42 * EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  ASSERT_EQ(0, EhFrameConstants::kEhFrameHdrSize % 4);
  ASSERT_EQ(0, EhFrameConstants::kEhFrameTerminatorSize % 4);
  EXPECT_EQ(0, (iterator.GetBufferSize() - EhFrameConstants::kEhFrameHdrSize -
                EhFrameConstants::kEhFrameTerminatorSize) %
                   kPointerSize);
}

TEST_F(EhFrameWriterTest, FDEHeader) {
  static const int kProcedureSize = 0x5678abcd;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.Finish(kProcedureSize);

  EhFrameIterator iterator = MakeIterator(&writer);
  int cie_size = iterator.GetNextUInt32();
  iterator.Skip(cie_size);

  int fde_size = iterator.GetNextUInt32();
  EXPECT_EQ(iterator.GetBufferSize(),
            fde_size + cie_size + EhFrameConstants::kEhFrameTerminatorSize +
                EhFrameConstants::kEhFrameHdrSize + 2 * kInt32Size);

  int backwards_offset_to_cie_offset = iterator.GetCurrentOffset();
  int backwards_offset_to_cie = iterator.GetNextUInt32();
  EXPECT_EQ(backwards_offset_to_cie_offset, backwards_offset_to_cie);

  int procedure_address_offset = iterator.GetCurrentOffset();
  int procedure_address = iterator.GetNextUInt32();
  EXPECT_EQ(-(procedure_address_offset + RoundUp(kProcedureSize, 8)),
            procedure_address);

  int procedure_size = iterator.GetNextUInt32();
  EXPECT_EQ(kProcedureSize, procedure_size);
}

TEST_F(EhFrameWriterTest, SetOffset) {
  static const int kOffset = 0x0badc0de;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.SetBaseAddressOffset(kOffset);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kDefCfaOffset,
            iterator.GetNextOpcode());
  EXPECT_EQ(kOffset, iterator.GetNextULeb128());
}

TEST_F(EhFrameWriterTest, IncreaseOffset) {
  static const int kFirstOffset = 121;
  static const int kSecondOffset = 16;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.SetBaseAddressOffset(kFirstOffset);
  writer.IncreaseBaseAddressOffset(kSecondOffset);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kDefCfaOffset,
            iterator.GetNextOpcode());
  EXPECT_EQ(kFirstOffset, iterator.GetNextULeb128());

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kDefCfaOffset,
            iterator.GetNextOpcode());
  EXPECT_EQ(kFirstOffset + kSecondOffset, iterator.GetNextULeb128());
}

TEST_F(EhFrameWriterTest, SetRegister) {
  Register test_register = Register::from_code(kTestRegisterCode);

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.SetBaseAddressRegister(test_register);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kDefCfaRegister,
            iterator.GetNextOpcode());
  EXPECT_EQ(kTestRegisterCode, iterator.GetNextULeb128());
}

TEST_F(EhFrameWriterTest, SetRegisterAndOffset) {
  Register test_register = Register::from_code(kTestRegisterCode);
  static const int kOffset = 0x0badc0de;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.SetBaseAddressRegisterAndOffset(test_register, kOffset);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kDefCfa, iterator.GetNextOpcode());
  EXPECT_EQ(kTestRegisterCode, iterator.GetNextULeb128());
  EXPECT_EQ(kOffset, iterator.GetNextULeb128());
}

TEST_F(EhFrameWriterTest, PcOffsetEncoding6bit) {
  static const int kOffset = 42;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(kOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ((1 << 6) | kOffset, iterator.GetNextByte());
}

TEST_F(EhFrameWriterTest, PcOffsetEncoding6bitDelta) {
  static const int kFirstOffset = 42;
  static const int kSecondOffset = 62;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(kFirstOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.AdvanceLocation(kSecondOffset *
                         EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ((1 << 6) | kFirstOffset, iterator.GetNextByte());
  EXPECT_EQ((1 << 6) | (kSecondOffset - kFirstOffset), iterator.GetNextByte());
}

TEST_F(EhFrameWriterTest, PcOffsetEncoding8bit) {
  static const int kOffset = 0x42;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(kOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kAdvanceLoc1,
            iterator.GetNextOpcode());
  EXPECT_EQ(kOffset, iterator.GetNextByte());
}

TEST_F(EhFrameWriterTest, PcOffsetEncoding8bitDelta) {
  static const int kFirstOffset = 0x10;
  static const int kSecondOffset = 0x70;
  static const int kThirdOffset = 0xb5;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(kFirstOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.AdvanceLocation(kSecondOffset *
                         EhFrameConstants::kCodeAlignmentFactor);
  writer.AdvanceLocation(kThirdOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ((1 << 6) | kFirstOffset, iterator.GetNextByte());

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kAdvanceLoc1,
            iterator.GetNextOpcode());
  EXPECT_EQ(kSecondOffset - kFirstOffset, iterator.GetNextByte());

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kAdvanceLoc1,
            iterator.GetNextOpcode());
  EXPECT_EQ(kThirdOffset - kSecondOffset, iterator.GetNextByte());
}

TEST_F(EhFrameWriterTest, PcOffsetEncoding16bit) {
  static const int kOffset = kMaxUInt8 + 42;
  ASSERT_LT(kOffset, kMaxUInt16);

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(kOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kAdvanceLoc2,
            iterator.GetNextOpcode());
  EXPECT_EQ(kOffset, iterator.GetNextUInt16());
}

TEST_F(EhFrameWriterTest, PcOffsetEncoding16bitDelta) {
  static const int kFirstOffset = 0x41;
  static const int kSecondOffset = kMaxUInt8 + 0x42;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(kFirstOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.AdvanceLocation(kSecondOffset *
                         EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kAdvanceLoc1,
            iterator.GetNextOpcode());
  EXPECT_EQ(kFirstOffset, iterator.GetNextByte());

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kAdvanceLoc2,
            iterator.GetNextOpcode());
  EXPECT_EQ(kSecondOffset - kFirstOffset, iterator.GetNextUInt16());
}

TEST_F(EhFrameWriterTest, PcOffsetEncoding32bit) {
  static const int kOffset = kMaxUInt16 + 42;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(kOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kAdvanceLoc4,
            iterator.GetNextOpcode());
  EXPECT_EQ(kOffset, iterator.GetNextUInt32());
}

TEST_F(EhFrameWriterTest, PcOffsetEncoding32bitDelta) {
  static const int kFirstOffset = kMaxUInt16 + 0x42;
  static const int kSecondOffset = kMaxUInt16 + 0x67;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.AdvanceLocation(kFirstOffset * EhFrameConstants::kCodeAlignmentFactor);
  writer.AdvanceLocation(kSecondOffset *
                         EhFrameConstants::kCodeAlignmentFactor);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kAdvanceLoc4,
            iterator.GetNextOpcode());
  EXPECT_EQ(kFirstOffset, iterator.GetNextUInt32());

  EXPECT_EQ((1 << 6) | (kSecondOffset - kFirstOffset), iterator.GetNextByte());
}

TEST_F(EhFrameWriterTest, SaveRegisterUnsignedOffset) {
  Register test_register = Register::from_code(kTestRegisterCode);
  static const int kOffset =
      EhFrameConstants::kDataAlignmentFactor > 0 ? 12344 : -12344;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.RecordRegisterSavedToStack(test_register, kOffset);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ((2 << 6) | kTestRegisterCode, iterator.GetNextByte());
  EXPECT_EQ(kOffset / EhFrameConstants::kDataAlignmentFactor,
            iterator.GetNextULeb128());
}

TEST_F(EhFrameWriterTest, SaveRegisterSignedOffset) {
  Register test_register = Register::from_code(kTestRegisterCode);
  static const int kOffset =
      EhFrameConstants::kDataAlignmentFactor < 0 ? 12344 : -12344;

  ASSERT_EQ(kOffset % EhFrameConstants::kDataAlignmentFactor, 0);

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.RecordRegisterSavedToStack(test_register, kOffset);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kOffsetExtendedSf,
            iterator.GetNextOpcode());
  EXPECT_EQ(kTestRegisterCode, iterator.GetNextULeb128());
  EXPECT_EQ(kOffset / EhFrameConstants::kDataAlignmentFactor,
            iterator.GetNextSLeb128());
}

TEST_F(EhFrameWriterTest, RegisterNotModified) {
  Register test_register = Register::from_code(kTestRegisterCode);

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.RecordRegisterNotModified(test_register);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ(EhFrameConstants::DwarfOpcodes::kSameValue,
            iterator.GetNextOpcode());
  EXPECT_EQ(kTestRegisterCode, iterator.GetNextULeb128());
}

TEST_F(EhFrameWriterTest, RegisterFollowsInitialRule) {
  Register test_register = Register::from_code(kTestRegisterCode);

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.RecordRegisterFollowsInitialRule(test_register);
  writer.Finish(100);

  EhFrameIterator iterator = MakeIterator(&writer);
  iterator.SkipToFdeDirectives();

  EXPECT_EQ((3 << 6) | kTestRegisterCode, iterator.GetNextByte());
}

TEST_F(EhFrameWriterTest, EhFrameHdrLayout) {
  static const int kCodeSize = 10;
  static const int kPaddingSize = 6;

  EhFrameWriter writer(zone());
  writer.Initialize();
  writer.Finish(kCodeSize);

  EhFrameIterator iterator = MakeIterator(&writer);

  // Skip the .eh_frame.

  int encoded_cie_size = iterator.GetNextUInt32();
  iterator.Skip(encoded_cie_size);
  int cie_size = encoded_cie_size + kInt32Size;

  int encoded_fde_size = iterator.GetNextUInt32();
  iterator.Skip(encoded_fde_size);
  int fde_size = encoded_fde_size + kInt32Size;

  iterator.Skip(EhFrameConstants::kEhFrameTerminatorSize);

  int eh_frame_size =
      cie_size + fde_size + EhFrameConstants::kEhFrameTerminatorSize;

  //
  // Plugging some numbers in the DSO layout shown in eh-frame.cc:
  //
  //  |      ...      |
  //  +---------------+ <-- (E) ---------
  //  |               |                 ^
  //  |  Instructions |  10 bytes       | .text
  //  |               |                 v
  //  +---------------+ <----------------
  //  |///////////////|
  //  |////Padding////|   6 bytes
  //  |///////////////|
  //  +---------------+ <---(D)----------
  //  |               |                 ^
  //  |      CIE      | cie_size bytes* |
  //  |               |                 |
  //  +---------------+ <-- (C)         |
  //  |               |                 | .eh_frame
  //  |      FDE      | fde_size bytes  |
  //  |               |                 |
  //  +---------------+                 |
  //  |   terminator  |   4 bytes       v
  //  +---------------+ <-- (B) ---------
  //  |    version    |                 ^
  //  +---------------+   4 bytes       |
  //  |   encoding    |                 |
  //  |  specifiers   |                 |
  //  +---------------+ <---(A)         | .eh_frame_hdr
  //  |   offset to   |                 |
  //  |   .eh_frame   |                 |
  //  +---------------+                 |
  //  |      ...      |                ...
  //
  //  (*) the size of the CIE is platform dependent.
  //

  int eh_frame_hdr_version = iterator.GetNextByte();
  EXPECT_EQ(EhFrameConstants::kEhFrameHdrVersion, eh_frame_hdr_version);

  // .eh_frame pointer encoding specifier.
  EXPECT_EQ(EhFrameConstants::kSData4 | EhFrameConstants::kPcRel,
            iterator.GetNextByte());

  // Lookup table size encoding specifier.
  EXPECT_EQ(EhFrameConstants::kUData4, iterator.GetNextByte());

  // Lookup table pointers encoding specifier.
  EXPECT_EQ(EhFrameConstants::kSData4 | EhFrameConstants::kDataRel,
            iterator.GetNextByte());

  // A -> D
  int offset_to_eh_frame = iterator.GetNextUInt32();
  EXPECT_EQ(-(EhFrameConstants::kFdeVersionSize +
              EhFrameConstants::kFdeEncodingSpecifiersSize + eh_frame_size),
            offset_to_eh_frame);

  int lut_entries = iterator.GetNextUInt32();
  EXPECT_EQ(1, lut_entries);

  // B -> E
  int offset_to_procedure = iterator.GetNextUInt32();
  EXPECT_EQ(-(eh_frame_size + kPaddingSize + kCodeSize), offset_to_procedure);

  // B -> C
  int offset_to_fde = iterator.GetNextUInt32();
  EXPECT_EQ(-(fde_size + EhFrameConstants::kEhFrameTerminatorSize),
            offset_to_fde);
}

#endif
