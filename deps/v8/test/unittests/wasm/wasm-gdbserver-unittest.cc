// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "src/debug/wasm/gdb-server/packet.h"
#include "src/debug/wasm/gdb-server/session.h"
#include "src/debug/wasm/gdb-server/transport.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

using ::testing::_;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::StrEq;

class WasmGdbRemoteTest : public ::testing::Test {};

TEST_F(WasmGdbRemoteTest, GdbRemotePacketAddChars) {
  Packet packet;

  // Read empty packet
  bool end_of_packet = packet.EndOfPacket();
  EXPECT_TRUE(end_of_packet);

  // Add raw chars
  packet.AddRawChar('4');
  packet.AddRawChar('2');

  std::string str;
  packet.GetString(&str);
  EXPECT_EQ("42", str);
}

TEST_F(WasmGdbRemoteTest, GdbRemotePacketAddBlock) {
  static const uint8_t block[] = {0x01, 0x02, 0x03, 0x04, 0x05,
                                  0x06, 0x07, 0x08, 0x09};
  static const size_t kLen = sizeof(block) / sizeof(uint8_t);
  Packet packet;
  packet.AddBlock(block, kLen);

  uint8_t buffer[kLen];
  bool ok = packet.GetBlock(buffer, kLen);
  EXPECT_TRUE(ok);
  EXPECT_EQ(0, memcmp(block, buffer, kLen));

  packet.Rewind();
  std::string str;
  ok = packet.GetString(&str);
  EXPECT_TRUE(ok);
  EXPECT_EQ("010203040506070809", str);
}

TEST_F(WasmGdbRemoteTest, GdbRemotePacketAddString) {
  Packet packet;
  packet.AddHexString("foobar");

  std::string str;
  bool ok = packet.GetString(&str);
  EXPECT_TRUE(ok);
  EXPECT_EQ("666f6f626172", str);

  packet.Clear();
  packet.AddHexString("GDB");
  ok = packet.GetString(&str);
  EXPECT_TRUE(ok);
  EXPECT_EQ("474442", str);
}

TEST_F(WasmGdbRemoteTest, GdbRemotePacketAddNumbers) {
  Packet packet;

  static const uint64_t u64_val = 0xdeadbeef89abcdef;
  static const uint8_t u8_val = 0x42;
  packet.AddNumberSep(u64_val, ';');
  packet.AddWord8(u8_val);

  std::string str;
  packet.GetString(&str);
  EXPECT_EQ("deadbeef89abcdef;42", str);

  packet.Rewind();
  uint64_t val = 0;
  char sep = '\0';
  bool ok = packet.GetNumberSep(&val, &sep);
  EXPECT_TRUE(ok);
  EXPECT_EQ(u64_val, val);
  uint8_t b = 0;
  ok = packet.GetWord8(&b);
  EXPECT_TRUE(ok);
  EXPECT_EQ(u8_val, b);
}

TEST_F(WasmGdbRemoteTest, GdbRemotePacketSequenceNumber) {
  Packet packet_with_sequence_num;
  packet_with_sequence_num.AddWord8(42);
  packet_with_sequence_num.AddRawChar(':');
  packet_with_sequence_num.AddHexString("foobar");

  int32_t sequence_num = 0;
  packet_with_sequence_num.ParseSequence();
  bool ok = packet_with_sequence_num.GetSequence(&sequence_num);
  EXPECT_TRUE(ok);
  EXPECT_EQ(42, sequence_num);

  Packet packet_without_sequence_num;
  packet_without_sequence_num.AddHexString("foobar");

  packet_without_sequence_num.ParseSequence();
  ok = packet_without_sequence_num.GetSequence(&sequence_num);
  EXPECT_FALSE(ok);
}

TEST_F(WasmGdbRemoteTest, GdbRemotePacketRunLengthEncoded) {
  Packet packet1;
  packet1.AddRawChar('0');
  packet1.AddRawChar('*');
  packet1.AddRawChar(' ');

  std::string str1;
  bool ok = packet1.GetHexString(&str1);
  EXPECT_TRUE(ok);
  EXPECT_EQ("0000", std::string(packet1.GetPayload()));

  Packet packet2;
  packet2.AddRawChar('1');
  packet2.AddRawChar('2');
  packet2.AddRawChar('3');
  packet2.AddRawChar('*');
  packet2.AddRawChar(' ');
  packet2.AddRawChar('a');
  packet2.AddRawChar('b');

  std::string str2;
  ok = packet2.GetHexString(&str2);
  EXPECT_TRUE(ok);
  EXPECT_EQ("123333ab", std::string(packet2.GetPayload()));
}

TEST_F(WasmGdbRemoteTest, GdbRemoteUtilStringSplit) {
  std::vector<std::string> parts1 = StringSplit({}, ",");
  EXPECT_EQ(size_t(0), parts1.size());

  auto parts2 = StringSplit("a", nullptr);
  EXPECT_EQ(size_t(1), parts2.size());
  EXPECT_EQ("a", parts2[0]);

  auto parts3 = StringSplit(";a;bc;def;", ",");
  EXPECT_EQ(size_t(1), parts3.size());
  EXPECT_EQ(";a;bc;def;", parts3[0]);

  auto parts4 = StringSplit(";a;bc;def;", ";");
  EXPECT_EQ(size_t(3), parts4.size());
  EXPECT_EQ("a", parts4[0]);
  EXPECT_EQ("bc", parts4[1]);
  EXPECT_EQ("def", parts4[2]);
}

class MockTransport : public TransportBase {
 public:
  MOCK_METHOD(bool, AcceptConnection, (), (override));
  MOCK_METHOD(bool, Read, (char*, int32_t), (override));
  MOCK_METHOD(bool, Write, (const char*, int32_t), (override));
  MOCK_METHOD(bool, IsDataAvailable, (), (const, override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, WaitForDebugStubEvent, (), (override));
  MOCK_METHOD(bool, SignalThreadEvent, (), (override));
};

TEST_F(WasmGdbRemoteTest, GdbRemoteSessionSendPacket) {
  const char* ack_buffer = "+";

  MockTransport mock_transport;
  EXPECT_CALL(mock_transport, Write(StrEq("$474442#39"), 10))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_transport, Read(_, _))
      .Times(1)
      .WillOnce(
          DoAll(SetArrayArgument<0>(ack_buffer, ack_buffer + 1), Return(true)));

  Session session(&mock_transport);

  Packet packet;
  packet.AddHexString("GDB");
  bool ok = session.SendPacket(&packet);
  EXPECT_TRUE(ok);
}

TEST_F(WasmGdbRemoteTest, GdbRemoteSessionSendPacketDisconnectOnNoAck) {
  MockTransport mock_transport;
  EXPECT_CALL(mock_transport, Write(StrEq("$474442#39"), 10))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_transport, Read(_, _)).Times(1).WillOnce(Return(false));
  EXPECT_CALL(mock_transport, Disconnect()).Times(1);

  Session session(&mock_transport);

  Packet packet;
  packet.AddHexString("GDB");
  bool ok = session.SendPacket(&packet);
  EXPECT_FALSE(ok);
}

TEST_F(WasmGdbRemoteTest, GdbRemoteSessionGetPacketCheckChecksum) {
  const char* buffer_bad = "$47#00";
  const char* buffer_ok = "$47#6b";

  MockTransport mock_transport;
  EXPECT_CALL(mock_transport, Read(_, _))
      .WillOnce(
          DoAll(SetArrayArgument<0>(buffer_bad, buffer_bad + 1), Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_bad + 1, buffer_bad + 2),
                      Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_bad + 2, buffer_bad + 3),
                      Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_bad + 3, buffer_bad + 4),
                      Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_bad + 4, buffer_bad + 5),
                      Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_bad + 5, buffer_bad + 6),
                      Return(true)))
      .WillOnce(
          DoAll(SetArrayArgument<0>(buffer_ok, buffer_ok + 1), Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_ok + 1, buffer_ok + 2),
                      Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_ok + 2, buffer_ok + 3),
                      Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_ok + 3, buffer_ok + 4),
                      Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_ok + 4, buffer_ok + 5),
                      Return(true)))
      .WillOnce(DoAll(SetArrayArgument<0>(buffer_ok + 5, buffer_ok + 6),
                      Return(true)));
  EXPECT_CALL(mock_transport, Write(StrEq("-"), 1))  // Signal bad packet
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_transport, Write(StrEq("+"), 1))  // Signal ack
      .Times(1)
      .WillOnce(Return(true));

  Session session(&mock_transport);

  Packet packet;
  bool ok = session.GetPacket(&packet);
  EXPECT_TRUE(ok);
  char ch;
  ok = packet.GetBlock(&ch, 1);
  EXPECT_TRUE(ok);
  EXPECT_EQ('G', ch);
}

TEST_F(WasmGdbRemoteTest, GdbRemoteSessionGetPacketDisconnectOnReadFailure) {
  MockTransport mock_transport;
  EXPECT_CALL(mock_transport, Read(_, _)).Times(1).WillOnce(Return(false));
  EXPECT_CALL(mock_transport, Disconnect()).Times(1);

  Session session(&mock_transport);
  Packet packet;
  bool ok = session.GetPacket(&packet);
  EXPECT_FALSE(ok);
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
