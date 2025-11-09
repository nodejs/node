#include "gtest/gtest.h"
#include "inspector/network_requests_buffer.h"

namespace node {
namespace inspector {

static uint8_t kDummyData[] = "long enough dummy data suitable for tests.";

TEST(RequestEntry, BufferSize) {
  auto entry = RequestEntry(0, Charset::kBinary, false, 10);
  EXPECT_EQ(entry.buffer_size(), 0u);
  entry.push_request_data_blob(protocol::Binary::fromSpan(kDummyData, 5));
  EXPECT_EQ(entry.buffer_size(), 5u);
  entry.push_response_data_blob(protocol::Binary::fromSpan(kDummyData, 3));
  EXPECT_EQ(entry.buffer_size(), 8u);

  // Exceeds the per-resource buffer size limit, ignore it.
  entry.push_request_data_blob(protocol::Binary::fromSpan(kDummyData, 3));
  EXPECT_EQ(entry.buffer_size(), 8u);
  entry.push_response_data_blob(protocol::Binary::fromSpan(kDummyData, 3));
  EXPECT_EQ(entry.buffer_size(), 8u);

  entry.clear_response_data_blobs();
  EXPECT_EQ(entry.buffer_size(), 5u);
}

TEST(RequestsBuffer, Basic) {
  RequestsBuffer buffer(10);
  EXPECT_FALSE(buffer.contains("1"));
  EXPECT_TRUE(
      buffer.emplace("1", RequestEntry(0, Charset::kBinary, false, 10)));
  EXPECT_TRUE(buffer.contains("1"));

  // Duplicate entry, ignore it.
  EXPECT_FALSE(
      buffer.emplace("1", RequestEntry(0, Charset::kBinary, false, 10)));
  EXPECT_TRUE(buffer.contains("1"));

  EXPECT_TRUE(
      buffer.emplace("2", RequestEntry(0, Charset::kBinary, false, 10)));
  EXPECT_TRUE(buffer.contains("2"));

  buffer.erase("1");
  EXPECT_FALSE(buffer.contains("1"));
  EXPECT_TRUE(buffer.contains("2"));
}

TEST(RequestsBuffer, Find) {
  RequestsBuffer buffer(10);
  EXPECT_FALSE(buffer.contains("1"));
  EXPECT_TRUE(
      buffer.emplace("1", RequestEntry(0, Charset::kBinary, false, 10)));
  EXPECT_TRUE(buffer.contains("1"));

  {
    auto it = buffer.find("1");
    EXPECT_NE(it, buffer.end());
    EXPECT_EQ(it->first, "1");
    EXPECT_EQ(it->second.buffer_size(), 0u);
    it->second.push_request_data_blob(
        protocol::Binary::fromSpan(kDummyData, 5));
    it->second.push_response_data_blob(
        protocol::Binary::fromSpan(kDummyData, 5));
    EXPECT_EQ(it->second.buffer_size(), 10u);

    EXPECT_EQ(buffer.total_buffer_size(), 0u);
    // Update the total buffer size when the iterator is destroyed.
  }
  EXPECT_EQ(buffer.total_buffer_size(), 10u);

  // Shrink
  {
    auto it = buffer.find("1");
    EXPECT_NE(it, buffer.end());
    EXPECT_EQ(it->first, "1");
    EXPECT_EQ(it->second.buffer_size(), 10u);
    it->second.clear_response_data_blobs();
    EXPECT_EQ(it->second.buffer_size(), 5u);
  }
  EXPECT_EQ(buffer.total_buffer_size(), 5u);

  // const find
  {
    auto it = buffer.cfind("1");
    EXPECT_NE(it, buffer.end());
    EXPECT_EQ(it->first, "1");
    EXPECT_EQ(it->second.buffer_size(), 5u);
  }

  // Non-existing entry.
  {
    auto it = buffer.find("2");
    EXPECT_EQ(it, buffer.end());
  }
  {
    auto it = buffer.cfind("2");
    EXPECT_EQ(it, buffer.end());
  }
}

TEST(RequestsBuffer, Erase) {
  RequestsBuffer buffer(10);
  EXPECT_FALSE(buffer.contains("1"));
  EXPECT_TRUE(
      buffer.emplace("1", RequestEntry(0, Charset::kBinary, false, 10)));
  EXPECT_TRUE(buffer.contains("1"));

  {
    auto it = buffer.find("1");
    EXPECT_NE(it, buffer.end());
    EXPECT_EQ(it->first, "1");
    EXPECT_EQ(it->second.buffer_size(), 0u);
    it->second.push_request_data_blob(
        protocol::Binary::fromSpan(kDummyData, 5));
    it->second.push_response_data_blob(
        protocol::Binary::fromSpan(kDummyData, 5));
    EXPECT_EQ(it->second.buffer_size(), 10u);

    EXPECT_EQ(buffer.total_buffer_size(), 0u);
    // Update the total buffer size when the iterator is destroyed.
  }
  EXPECT_EQ(buffer.total_buffer_size(), 10u);

  buffer.erase("1");
  EXPECT_FALSE(buffer.contains("1"));
  EXPECT_EQ(buffer.total_buffer_size(), 0u);

  // Non-existing entry.
  buffer.erase("2");
  EXPECT_EQ(buffer.total_buffer_size(), 0u);
}

TEST(RequestsBuffer, EnforceLimit) {
  RequestsBuffer buffer(15);
  {
    EXPECT_TRUE(
        buffer.emplace("1", RequestEntry(0, Charset::kBinary, false, 15)));
    buffer.find("1")->second.push_request_data_blob(
        protocol::Binary::fromSpan(kDummyData, 10));
    EXPECT_EQ(buffer.total_buffer_size(), 10u);
  }
  {
    EXPECT_TRUE(
        buffer.emplace("2", RequestEntry(0, Charset::kBinary, false, 15)));
    buffer.find("2")->second.push_request_data_blob(
        protocol::Binary::fromSpan(kDummyData, 5));
    EXPECT_EQ(buffer.total_buffer_size(), 15u);
  }

  // Exceeds the limit, the oldest entry is removed.
  {
    EXPECT_TRUE(
        buffer.emplace("3", RequestEntry(0, Charset::kBinary, false, 15)));
    buffer.find("3")->second.push_request_data_blob(
        protocol::Binary::fromSpan(kDummyData, 5));
    // "2" and "3" are kept.
    EXPECT_EQ(buffer.total_buffer_size(), 10u);
  }

  EXPECT_FALSE(buffer.contains("1"));
  EXPECT_TRUE(buffer.contains("2"));
  EXPECT_TRUE(buffer.contains("3"));

  // Exceeds the limit, the oldest entry is removed.
  {
    EXPECT_TRUE(
        buffer.emplace("4", RequestEntry(0, Charset::kBinary, false, 15)));
    buffer.find("4")->second.push_request_data_blob(
        protocol::Binary::fromSpan(kDummyData, 10));
    // "3" and "4" are kept.
    EXPECT_EQ(buffer.total_buffer_size(), 15u);
  }

  EXPECT_FALSE(buffer.contains("1"));
  EXPECT_FALSE(buffer.contains("2"));
  EXPECT_TRUE(buffer.contains("3"));
  EXPECT_TRUE(buffer.contains("4"));
}

}  // namespace inspector
}  // namespace node
