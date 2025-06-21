// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-module-sourcemap.h"
#include <memory>

#include "src/api/api.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmModuleSourceMapTest : public TestWithIsolateAndZone {};

TEST_F(WasmModuleSourceMapTest, InvalidSourceMap) {
  auto i_isolate = isolate();
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(i_isolate);

  // Incomplete source map without "sources" entry.
  char incomplete_src_map[] =
      "{\"version\":3,\"names\":[],\"mappings\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto incomplete_src_map_str =
      v8::String::NewFromUtf8(v8_isolate, incomplete_src_map).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> incomplete_src_map_ptr(
      new WasmModuleSourceMap(v8_isolate, incomplete_src_map_str));
  EXPECT_FALSE(incomplete_src_map_ptr->IsValid());

  // Miswrite key "mappings" as "mapping".
  char wrong_key[] =
      "{\"version\":3,\"sources\":[\"./"
      "test.h\",\"main.cpp\"],\"names\":[],\"mapping\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto wrong_key_str =
      v8::String::NewFromUtf8(v8_isolate, wrong_key).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> wrong_key_ptr(
      new WasmModuleSourceMap(v8_isolate, wrong_key_str));
  EXPECT_FALSE(wrong_key_ptr->IsValid());

  // Wrong version number.
  char wrong_ver[] =
      "{\"version\":2,\"sources\":[\"./"
      "test.h\",\"main.cpp\"],\"names\":[],\"mappings\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto wrong_ver_str =
      v8::String::NewFromUtf8(v8_isolate, wrong_ver).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> wrong_ver_ptr(
      new WasmModuleSourceMap(v8_isolate, wrong_ver_str));
  EXPECT_FALSE(wrong_ver_ptr->IsValid());

  // Wrong type of "version" entry.
  char ver_as_arr[] =
      "{\"version\":[3],\"sources\":[\"./"
      "test.h\",\"main.cpp\"],\"names\":[],\"mappings\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto ver_as_arr_str =
      v8::String::NewFromUtf8(v8_isolate, ver_as_arr).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> ver_as_arr_ptr(
      new WasmModuleSourceMap(v8_isolate, ver_as_arr_str));
  EXPECT_FALSE(ver_as_arr_ptr->IsValid());

  // Wrong type of "sources" entry.
  char sources_as_str[] =
      "{\"version\":3,\"sources\":\"./"
      "test.h,main.cpp\",\"names\":[],\"mappings\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto sources_as_str_str =
      v8::String::NewFromUtf8(v8_isolate, sources_as_str).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> sources_as_str_ptr(
      new WasmModuleSourceMap(v8_isolate, sources_as_str_str));
  EXPECT_FALSE(sources_as_str_ptr->IsValid());

  // Invalid "mappings" entry.
  char wrong_mappings[] =
      "{\"version\":3,\"sources\":[\"./"
      "test.h\",\"main.cpp\"],\"names\":[],\"mappings\":\"6/"
      "&BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto wrong_mappings_str =
      v8::String::NewFromUtf8(v8_isolate, wrong_mappings).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> wrong_mappings_ptr(
      new WasmModuleSourceMap(v8_isolate, wrong_mappings_str));
  EXPECT_FALSE(wrong_mappings_ptr->IsValid());
}

TEST_F(WasmModuleSourceMapTest, HasSource) {
  char src_map[] =
      "{\"version\":3,\"sources\":[\"./"
      "test.h\",\"main.cpp\"],\"names\":[],\"mappings\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto i_isolate = isolate();
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(i_isolate);
  auto src_map_str =
      v8::String::NewFromUtf8(v8_isolate, src_map).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> src_map_ptr(
      new WasmModuleSourceMap(v8_isolate, src_map_str));
  EXPECT_TRUE(src_map_ptr->IsValid());

  EXPECT_FALSE(src_map_ptr->HasSource(0x387, 0x3AF));
  EXPECT_FALSE(src_map_ptr->HasSource(0x3B0, 0x3B5));
  EXPECT_FALSE(src_map_ptr->HasSource(0x3B6, 0x3BC));
  EXPECT_FALSE(src_map_ptr->HasSource(0x3BD, 0x3C7));
  EXPECT_FALSE(src_map_ptr->HasSource(0x3C8, 0x3DA));
  EXPECT_TRUE(src_map_ptr->HasSource(0x3DB, 0x414));
  EXPECT_TRUE(src_map_ptr->HasSource(0x415, 0x44E));
  EXPECT_TRUE(src_map_ptr->HasSource(0x450, 0x4DC));
  EXPECT_TRUE(src_map_ptr->HasSource(0x4DE, 0x5F1));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5F3, 0x437A));
  EXPECT_FALSE(src_map_ptr->HasSource(0x437C, 0x5507));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5508, 0x5557));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5559, 0x5609));
  EXPECT_FALSE(src_map_ptr->HasSource(0x560A, 0x563D));
  EXPECT_FALSE(src_map_ptr->HasSource(0x563E, 0x564A));
  EXPECT_FALSE(src_map_ptr->HasSource(0x564B, 0x5656));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5658, 0x5713));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5715, 0x59B0));
  EXPECT_FALSE(src_map_ptr->HasSource(0x59B1, 0x59BC));
  EXPECT_FALSE(src_map_ptr->HasSource(0x59BD, 0x59C6));
  EXPECT_FALSE(src_map_ptr->HasSource(0x59C7, 0x59D8));
  EXPECT_FALSE(src_map_ptr->HasSource(0x59D9, 0x59E7));
  EXPECT_FALSE(src_map_ptr->HasSource(0x59E9, 0x5B50));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5B52, 0x5C53));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5C54, 0x5C57));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5C59, 0x5EBD));
  EXPECT_FALSE(src_map_ptr->HasSource(0x5EBF, 0x6030));
  EXPECT_FALSE(src_map_ptr->HasSource(0x6031, 0x608D));
  EXPECT_FALSE(src_map_ptr->HasSource(0x608E, 0x609E));
  EXPECT_FALSE(src_map_ptr->HasSource(0x609F, 0x60B3));
  EXPECT_FALSE(src_map_ptr->HasSource(0x60B4, 0x60BD));
}

TEST_F(WasmModuleSourceMapTest, HasValidEntry) {
  char src_map[] =
      "{\"version\":3,\"sources\":[\"./"
      "test.h\",\"main.cpp\"],\"names\":[],\"mappings\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto i_isolate = isolate();
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(i_isolate);
  auto src_map_str =
      v8::String::NewFromUtf8(v8_isolate, src_map).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> src_map_ptr(
      new WasmModuleSourceMap(v8_isolate, src_map_str));
  EXPECT_TRUE(src_map_ptr->IsValid());

  EXPECT_FALSE(src_map_ptr->HasValidEntry(0x450, 0x467));
  EXPECT_FALSE(src_map_ptr->HasValidEntry(0x450, 0x450));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x450, 0x47A));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x450, 0x4A9));
  EXPECT_FALSE(src_map_ptr->HasValidEntry(0x4DE, 0x4F5));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x4DE, 0x541));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x4DE, 0x57D));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x4DE, 0x5B7));
  EXPECT_FALSE(src_map_ptr->HasValidEntry(0x4DE, 0x4DE));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x4DE, 0x500));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x4DE, 0x521));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x4DE, 0x560));
  EXPECT_TRUE(src_map_ptr->HasValidEntry(0x4DE, 0x597));
}

TEST_F(WasmModuleSourceMapTest, GetFilename) {
  char src_map[] =
      "{\"version\":3,\"sources\":[\"./"
      "test.h\",\"main.cpp\"],\"names\":[],\"mappings\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto i_isolate = isolate();
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(i_isolate);
  auto src_map_str =
      v8::String::NewFromUtf8(v8_isolate, src_map).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> src_map_ptr(
      new WasmModuleSourceMap(v8_isolate, src_map_str));
  EXPECT_TRUE(src_map_ptr->IsValid());

  EXPECT_STREQ("./test.h", src_map_ptr->GetFilename(0x47A).c_str());
  EXPECT_STREQ("./test.h", src_map_ptr->GetFilename(0x4A9).c_str());
  EXPECT_STREQ("main.cpp", src_map_ptr->GetFilename(0x500).c_str());
  EXPECT_STREQ("main.cpp", src_map_ptr->GetFilename(0x521).c_str());
  EXPECT_STREQ("main.cpp", src_map_ptr->GetFilename(0x541).c_str());
  EXPECT_STREQ("main.cpp", src_map_ptr->GetFilename(0x560).c_str());
  EXPECT_STREQ("main.cpp", src_map_ptr->GetFilename(0x57D).c_str());
  EXPECT_STREQ("main.cpp", src_map_ptr->GetFilename(0x597).c_str());
  EXPECT_STREQ("main.cpp", src_map_ptr->GetFilename(0x5B7).c_str());
}

TEST_F(WasmModuleSourceMapTest, SourceLine) {
  char src_map[] =
      "{\"version\":3,\"sources\":[\"./"
      "test.h\",\"main.cpp\"],\"names\":[],\"mappings\":\"6/"
      "BAGA,0DAIA,2DAIA,IAEA,+BACA,wCADA,mBAGA,4CCXA,6BACA,IACA,4BACA,gBADA,"
      "mBAIA,4BACA,QADA,mBAIA,4BACA,gBADA,mBAVA,mBAcA\"}";
  auto i_isolate = isolate();
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(i_isolate);
  auto src_map_str =
      v8::String::NewFromUtf8(v8_isolate, src_map).ToLocalChecked();
  std::unique_ptr<WasmModuleSourceMap> src_map_ptr(
      new WasmModuleSourceMap(v8_isolate, src_map_str));
  EXPECT_TRUE(src_map_ptr->IsValid());

  EXPECT_EQ(13ul, src_map_ptr->GetSourceLine(0x47A));
  EXPECT_EQ(14ul, src_map_ptr->GetSourceLine(0x4A9));
  EXPECT_EQ(5ul, src_map_ptr->GetSourceLine(0x500));
  EXPECT_EQ(7ul, src_map_ptr->GetSourceLine(0x521));
  EXPECT_EQ(8ul, src_map_ptr->GetSourceLine(0x541));
  EXPECT_EQ(11ul, src_map_ptr->GetSourceLine(0x560));
  EXPECT_EQ(12ul, src_map_ptr->GetSourceLine(0x57D));
  EXPECT_EQ(15ul, src_map_ptr->GetSourceLine(0x597));
  EXPECT_EQ(16ul, src_map_ptr->GetSourceLine(0x5B7));
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
