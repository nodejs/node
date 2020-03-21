#include "snapshot_support-inl.h"
#include "gtest/gtest.h"

TEST(SnapshotSupportTest, DumpWithoutErrors) {
  node::SnapshotCreateData data_w(nullptr);

  data_w.StartWriteEntry("Outer");
  data_w.WriteUint32(10);
  data_w.StartWriteEntry("Inner");
  data_w.WriteString("Hello!");
  data_w.EndWriteEntry();
  data_w.EndWriteEntry();

  node::SnapshotReadData data_r(data_w.storage());

  std::vector<node::SnapshotReadData::DumpLine> lines;
  std::vector<node::SnapshotDataBase::Error> errors;
  std::tie(lines, errors) = data_r.Dump();

  EXPECT_TRUE(errors.empty());
  EXPECT_EQ(lines.size(), 6u);
  while (lines.size() < 6) lines.emplace_back();  // Avoid OOB test crahes.
  EXPECT_EQ(lines[0].index, 0u);
  EXPECT_EQ(lines[0].depth, 0u);
  EXPECT_EQ(lines[0].description, "StartEntry: [Outer]");
  EXPECT_EQ(lines[1].depth, 1u);
  EXPECT_EQ(lines[1].description, "Uint32: 10");
  EXPECT_EQ(lines[2].depth, 1u);
  EXPECT_EQ(lines[2].description, "StartEntry: [Inner]");
  EXPECT_EQ(lines[3].depth, 2u);
  EXPECT_EQ(lines[3].description, "String: 'Hello!'");
  EXPECT_EQ(lines[4].depth, 2u);
  EXPECT_EQ(lines[4].description, "EndEntry");
  EXPECT_EQ(lines[5].depth, 1u);
  EXPECT_EQ(lines[5].description, "EndEntry");
}

TEST(SnapshotSupportTest, DumpWithErrors) {
  node::SnapshotCreateData data_w(nullptr);

  data_w.StartWriteEntry("Outer");
  data_w.WriteUint32(10);
  data_w.StartWriteEntry("Inner");
  data_w.WriteString("Hello!");
  data_w.EndWriteEntry();
  data_w.EndWriteEntry();

  std::vector<uint8_t> storage = data_w.storage();
  storage.at(21)++;  // Invalidate storage data in some way.
  node::SnapshotReadData data_r(std::move(storage));

  data_r.DumpToStderr();
  std::vector<node::SnapshotReadData::DumpLine> lines;
  std::vector<node::SnapshotDataBase::Error> errors;
  std::tie(lines, errors) = data_r.Dump();

  // The expectations here may need to be updated if the snapshot format
  // changes.
  EXPECT_EQ(lines.size(), 5u);
  while (lines.size() < 5) lines.emplace_back();  // Avoid OOB test crahes.
  EXPECT_EQ(lines[0].index, 0u);
  EXPECT_EQ(lines[0].depth, 0u);
  EXPECT_EQ(lines[0].description, "StartEntry: [Outer]");
  EXPECT_EQ(lines[1].depth, 1u);
  EXPECT_EQ(lines[1].description, "Uint32: 10");
  EXPECT_EQ(lines[2].depth, 1u);
  EXPECT_EQ(lines[2].description, "EndEntry");
  EXPECT_EQ(lines[3].depth, 0u);
  EXPECT_EQ(lines[3].description, "String: 'Inner'");
  EXPECT_EQ(lines[4].depth, 0u);
  EXPECT_EQ(lines[4].description, "String: 'Hello!'");

  EXPECT_EQ(errors.size(), 1u);
  while (errors.size() < 1) errors.emplace_back();  // Avoid OOB test crashes.
  EXPECT_EQ(errors[0].message,
      "At [54]  Attempting to end entry on empty stack, more EndReadEntry() "
      "than StartReadEntry() calls");
}
