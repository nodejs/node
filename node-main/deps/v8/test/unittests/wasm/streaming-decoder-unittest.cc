// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/objects/objects-inl.h"

#include "src/wasm/module-decoder.h"
#include "src/wasm/streaming-decoder.h"

#include "src/objects/descriptor-array.h"
#include "src/objects/dictionary.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

struct MockStreamingResult {
  size_t num_sections = 0;
  size_t num_functions = 0;
  bool error;
  base::OwnedVector<const uint8_t> received_bytes;

  bool ok() const { return !error; }

  MockStreamingResult() = default;
};

class NoTracer {
 public:
  void Bytes(const uint8_t* start, uint32_t count) {}
  void Description(const char* desc) {}
};

class MockStreamingProcessor : public StreamingProcessor {
 public:
  explicit MockStreamingProcessor(MockStreamingResult* result)
      : result_(result) {}

  bool ProcessModuleHeader(base::Vector<const uint8_t> bytes) override {
    Decoder decoder(bytes.begin(), bytes.end());
    uint32_t magic_word = decoder.consume_u32("wasm magic", ITracer::NoTrace);
    if (decoder.failed() || magic_word != kWasmMagic) {
      result_->error = WasmError(0, "expected wasm magic");
      return false;
    }
    uint32_t magic_version =
        decoder.consume_u32("wasm version", ITracer::NoTrace);
    if (decoder.failed() || magic_version != kWasmVersion) {
      result_->error = WasmError(4, "expected wasm version");
      return false;
    }
    return true;
  }

  // Process all sections but the code section.
  bool ProcessSection(SectionCode section_code,
                      base::Vector<const uint8_t> bytes,
                      uint32_t offset) override {
    ++result_->num_sections;
    return true;
  }

  bool ProcessCodeSectionHeader(int num_functions, uint32_t offset,
                                std::shared_ptr<WireBytesStorage>,
                                int code_section_start,
                                int code_section_length) override {
    return true;
  }

  // Process a function body.
  bool ProcessFunctionBody(base::Vector<const uint8_t> bytes,
                           uint32_t offset) override {
    ++result_->num_functions;
    return true;
  }

  void OnFinishedChunk() override {}

  // Finish the processing of the stream.
  void OnFinishedStream(base::OwnedVector<const uint8_t> bytes,
                        bool after_error) override {
    result_->received_bytes = std::move(bytes);
    result_->error = after_error;
  }

  void OnAbort() override {}

  bool Deserialize(base::Vector<const uint8_t> module_bytes,
                   base::Vector<const uint8_t> wire_bytes) override {
    return false;
  }

 private:
  MockStreamingResult* const result_;
};

class WasmStreamingDecoderTest : public ::testing::Test {
 public:
  void ExpectVerifies(base::Vector<const uint8_t> data,
                      size_t expected_sections, size_t expected_functions) {
    for (int split = 0; split <= data.length(); ++split) {
      MockStreamingResult result;
      auto stream = StreamingDecoder::CreateAsyncStreamingDecoder(
          std::make_unique<MockStreamingProcessor>(&result));
      stream->OnBytesReceived(data.SubVector(0, split));
      stream->OnBytesReceived(data.SubVector(split, data.length()));
      stream->Finish();
      EXPECT_TRUE(result.ok());
      EXPECT_EQ(expected_sections, result.num_sections);
      EXPECT_EQ(expected_functions, result.num_functions);
      EXPECT_EQ(data, result.received_bytes.as_vector());
    }
  }

  void ExpectFailure(base::Vector<const uint8_t> data) {
    for (int split = 0; split <= data.length(); ++split) {
      MockStreamingResult result;
      auto stream = StreamingDecoder::CreateAsyncStreamingDecoder(
          std::make_unique<MockStreamingProcessor>(&result));
      stream->OnBytesReceived(data.SubVector(0, split));
      stream->OnBytesReceived(data.SubVector(split, data.length()));
      stream->Finish();
      EXPECT_FALSE(result.ok());
      EXPECT_TRUE(result.error);
    }
  }
};

TEST_F(WasmStreamingDecoderTest, EmptyStream) {
  MockStreamingResult result;
  auto stream = StreamingDecoder::CreateAsyncStreamingDecoder(
      std::make_unique<MockStreamingProcessor>(&result));
  stream->Finish();
  EXPECT_FALSE(result.ok());
}

TEST_F(WasmStreamingDecoderTest, IncompleteModuleHeader) {
  const uint8_t data[] = {U32_LE(kWasmMagic), U32_LE(kWasmVersion)};
  {
    MockStreamingResult result;
    auto stream = StreamingDecoder::CreateAsyncStreamingDecoder(
        std::make_unique<MockStreamingProcessor>(&result));
    stream->OnBytesReceived(base::VectorOf(data, 1));
    stream->Finish();
    EXPECT_FALSE(result.ok());
  }
  for (uint32_t length = 1; length < sizeof(data); ++length) {
    ExpectFailure(base::VectorOf(data, length));
  }
}

TEST_F(WasmStreamingDecoderTest, MagicAndVersion) {
  const uint8_t data[] = {U32_LE(kWasmMagic), U32_LE(kWasmVersion)};
  ExpectVerifies(base::ArrayVector(data), 0, 0);
}

TEST_F(WasmStreamingDecoderTest, BadMagic) {
  for (uint32_t x = 1; x; x <<= 1) {
    const uint8_t data[] = {U32_LE(kWasmMagic ^ x), U32_LE(kWasmVersion)};
    ExpectFailure(base::ArrayVector(data));
  }
}

TEST_F(WasmStreamingDecoderTest, BadVersion) {
  for (uint32_t x = 1; x; x <<= 1) {
    const uint8_t data[] = {U32_LE(kWasmMagic), U32_LE(kWasmVersion ^ x)};
    ExpectFailure(base::ArrayVector(data));
  }
}

TEST_F(WasmStreamingDecoderTest, OneSection) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x6,                   // Section Length
      0x0,                   // Payload
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0                    // 6
  };
  ExpectVerifies(base::ArrayVector(data), 1, 0);
}

TEST_F(WasmStreamingDecoderTest, OneSection_b) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x86,                  // Section Length = 6 (LEB)
      0x0,                   // --
      0x0,                   // Payload
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0                    // 6
  };
  ExpectVerifies(base::ArrayVector(data), 1, 0);
}

TEST_F(WasmStreamingDecoderTest, OneShortSection) {
  // Short section means that section length + payload is less than 5 bytes,
  // which is the maximum size of the length field.
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x2,                   // Section Length
      0x0,                   // Payload
      0x0                    // 2
  };
  ExpectVerifies(base::ArrayVector(data), 1, 0);
}

TEST_F(WasmStreamingDecoderTest, OneShortSection_b) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x82,                  // Section Length = 2 (LEB)
      0x80,                  // --
      0x0,                   // --
      0x0,                   // Payload
      0x0                    // 2
  };
  ExpectVerifies(base::ArrayVector(data), 1, 0);
}

TEST_F(WasmStreamingDecoderTest, OneEmptySection) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x0                    // Section Length
  };
  ExpectVerifies(base::ArrayVector(data), 1, 0);
}

TEST_F(WasmStreamingDecoderTest, OneSectionNotEnoughPayload1) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x6,                   // Section Length
      0x0,                   // Payload
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0                    // 5
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, OneSectionNotEnoughPayload2) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x6,                   // Section Length
      0x0                    // Payload
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, OneSectionInvalidLength) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x80,                  // Section Length (invalid LEB)
      0x80,                  // --
      0x80,                  // --
      0x80,                  // --
      0x80,                  // --
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, TwoLongSections) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x6,                   // Section Length
      0x0,                   // Payload
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x2,                   // Section ID
      0x7,                   // Section Length
      0x0,                   // Payload
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x0                    // 7
  };
  ExpectVerifies(base::ArrayVector(data), 2, 0);
}

TEST_F(WasmStreamingDecoderTest, TwoShortSections) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x1,                   // Section Length
      0x0,                   // Payload
      0x2,                   // Section ID
      0x2,                   // Section Length
      0x0,                   // Payload
      0x0,                   // 2
  };
  ExpectVerifies(base::ArrayVector(data), 2, 0);
}

TEST_F(WasmStreamingDecoderTest, TwoSectionsShortLong) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x1,                   // Section Length
      0x0,                   // Payload
      0x2,                   // Section ID
      0x7,                   // Section Length
      0x0,                   // Payload
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x0                    // 7
  };
  ExpectVerifies(base::ArrayVector(data), 2, 0);
}

TEST_F(WasmStreamingDecoderTest, TwoEmptySections) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      0x1,                   // Section ID
      0x0,                   // Section Length
      0x2,                   // Section ID
      0x0                    // Section Length
  };
  ExpectVerifies(base::ArrayVector(data), 2, 0);
}

TEST_F(WasmStreamingDecoderTest, OneFunction) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x8,                   // Section Length
      0x1,                   // Number of Functions
      0x6,                   // Function Length
      0x0,                   // Function
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
  };
  ExpectVerifies(base::ArrayVector(data), 0, 1);
}

TEST_F(WasmStreamingDecoderTest, OneShortFunction) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x3,                   // Section Length
      0x1,                   // Number of Functions
      0x1,                   // Function Length
      0x0,                   // Function
  };
  ExpectVerifies(base::ArrayVector(data), 0, 1);
}

TEST_F(WasmStreamingDecoderTest, EmptyFunction) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x2,                   // Section Length
      0x1,                   // Number of Functions
      0x0,                   // Function Length  -- ERROR
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, TwoFunctions) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x10,                  // Section Length
      0x2,                   // Number of Functions
      0x6,                   // Function Length
      0x0,                   // Function
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x7,                   // Function Length
      0x0,                   // Function
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x0,                   // 7
  };
  ExpectVerifies(base::ArrayVector(data), 0, 2);
}

TEST_F(WasmStreamingDecoderTest, TwoFunctions_b) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0xB,                   // Section Length
      0x2,                   // Number of Functions
      0x1,                   // Function Length
      0x0,                   // Function
      0x7,                   // Function Length
      0x0,                   // Function
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x0,                   // 7
  };
  ExpectVerifies(base::ArrayVector(data), 0, 2);
}

TEST_F(WasmStreamingDecoderTest, CodeSectionLengthZero) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x0,                   // Section Length
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, CodeSectionLengthTooHigh) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0xD,                   // Section Length
      0x2,                   // Number of Functions
      0x7,                   // Function Length
      0x0,                   // Function
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x0,                   // 7
      0x1,                   // Function Length
      0x0,                   // Function
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, CodeSectionLengthTooHighZeroFunctions) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0xD,                   // Section Length
      0x0,                   // Number of Functions
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, CodeSectionLengthTooLow) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x9,                   // Section Length
      0x2,                   // Number of Functions  <0>
      0x7,                   // Function Length      <1>
      0x0,                   // Function             <2>
      0x0,                   // 2                    <3>
      0x0,                   // 3                    <3>
      0x0,                   // 4                    <4>
      0x0,                   // 5                    <5>
      0x0,                   // 6                    <6>
      0x0,                   // 7                    <7>
      0x1,                   // Function Length      <8> -- ERROR
      0x0,                   // Function
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, CodeSectionLengthTooLowEndsInNumFunctions) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x1,                   // Section Length
      0x82,                  // Number of Functions  <0>
      0x80,                  // --                   <1> -- ERROR
      0x00,                  // --
      0x7,                   // Function Length
      0x0,                   // Function
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x0,                   // 7
      0x1,                   // Function Length
      0x0,                   // Function
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, CodeSectionLengthTooLowEndsInFunctionLength) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x5,                   // Section Length
      0x82,                  // Number of Functions  <0>
      0x80,                  // --                   <1>
      0x00,                  // --                   <2>
      0x87,                  // Function Length      <3>
      0x80,                  // --                   <4>
      0x00,                  // --                   <5> -- ERROR
      0x0,                   // Function
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x0,                   // 7
      0x1,                   // Function Length
      0x0,                   // Function
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, NumberOfFunctionsTooHigh) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0xB,                   // Section Length
      0x4,                   // Number of Functions
      0x7,                   // Function Length
      0x0,                   // Function
      0x0,                   // 2
      0x0,                   // 3
      0x0,                   // 4
      0x0,                   // 5
      0x0,                   // 6
      0x0,                   // 7
      0x1,                   // Function Length
      0x0,                   // Function
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, NumberOfFunctionsTooLow) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x8,                   // Section Length
      0x2,                   // Number of Functions
      0x1,                   // Function Length
      0x0,                   // Function
      0x2,                   // Function Length
      0x0,                   // Function byte#0
      0x0,                   // Function byte#1   -- ERROR
      0x1,                   // Function Length
      0x0                    // Function
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, TwoCodeSections) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x3,                   // Section Length
      0x1,                   // Number of Functions
      0x1,                   // Function Length
      0x0,                   // Function
      kCodeSectionCode,      // Section ID      -- ERROR
      0x3,                   // Section Length
      0x1,                   // Number of Functions
      0x1,                   // Function Length
      0x0,                   // Function
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, UnknownSection) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x3,                   // Section Length
      0x1,                   // Number of Functions
      0x1,                   // Function Length
      0x0,                   // Function
      kUnknownSectionCode,   // Section ID
      0x3,                   // Section Length
      0x1,                   // Name Length
      0x1,                   // Name
      0x0,                   // Content
  };
  ExpectVerifies(base::ArrayVector(data), 1, 1);
}

TEST_F(WasmStreamingDecoderTest, UnknownSectionSandwich) {
  const uint8_t data[] = {
      U32_LE(kWasmMagic),    // --
      U32_LE(kWasmVersion),  // --
      kCodeSectionCode,      // Section ID
      0x3,                   // Section Length
      0x1,                   // Number of Functions
      0x1,                   // Function Length
      0x0,                   // Function
      kUnknownSectionCode,   // Section ID
      0x3,                   // Section Length
      0x1,                   // Name Length
      0x1,                   // Name
      0x0,                   // Content
      kCodeSectionCode,      // Section ID     -- ERROR
      0x3,                   // Section Length
      0x1,                   // Number of Functions
      0x1,                   // Function Length
      0x0,                   // Function
  };
  ExpectFailure(base::ArrayVector(data));
}

TEST_F(WasmStreamingDecoderTest, InvalidSectionCode) {
  uint8_t kInvalidSectionCode = 61;
  const uint8_t data[] = {WASM_MODULE_HEADER, SECTION(Invalid)};
  ExpectFailure(base::ArrayVector(data));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
