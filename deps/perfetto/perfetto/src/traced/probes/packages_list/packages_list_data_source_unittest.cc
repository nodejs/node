/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/traced/probes/packages_list/packages_list_data_source.h"

#include <stdio.h>

#include <set>
#include <string>

#include "perfetto/ext/base/pipe.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "protos/perfetto/trace/android/packages_list.gen.h"
#include "protos/perfetto/trace/android/packages_list.pbzero.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

TEST(PackagesListDataSourceTest, ParseLineNonProfileNonDebug) {
  char kLine[] =
      "com.test.app 1234 0 /data/user/0/com.test.app "
      "default:targetSdkVersion=12452 1234,5678 0 1111\n";
  Package pkg;
  ASSERT_TRUE(ReadPackagesListLine(kLine, &pkg));
  EXPECT_EQ(pkg.name, "com.test.app");
  EXPECT_EQ(pkg.debuggable, false);
  EXPECT_EQ(pkg.profileable_from_shell, false);
  EXPECT_EQ(pkg.version_code, 1111);
}

TEST(PackagesListDataSourceTest, ParseLineProfileNonDebug) {
  char kLine[] =
      "com.test.app 1234 0 /data/user/0/com.test.app "
      "default:targetSdkVersion=12452 1234,5678 1 1111\n";
  Package pkg;
  ASSERT_TRUE(ReadPackagesListLine(kLine, &pkg));
  EXPECT_EQ(pkg.name, "com.test.app");
  EXPECT_EQ(pkg.debuggable, false);
  EXPECT_EQ(pkg.profileable_from_shell, true);
  EXPECT_EQ(pkg.version_code, 1111);
}

TEST(PackagesListDataSourceTest, ParseLineNonProfileDebug) {
  char kLine[] =
      "com.test.app 1234 1 /data/user/0/com.test.app "
      "default:targetSdkVersion=12452 1234,5678 0 1111\n";
  Package pkg;
  ASSERT_TRUE(ReadPackagesListLine(kLine, &pkg));
  EXPECT_EQ(pkg.name, "com.test.app");
  EXPECT_EQ(pkg.debuggable, true);
  EXPECT_EQ(pkg.profileable_from_shell, false);
  EXPECT_EQ(pkg.version_code, 1111);
}

TEST(PackagesListDataSourceTest, ParseLineProfileDebug) {
  char kLine[] =
      "com.test.app 1234 1 /data/user/0/com.test.app "
      "default:targetSdkVersion=12452 1234,5678 1 1111\n";
  Package pkg;
  ASSERT_TRUE(ReadPackagesListLine(kLine, &pkg));
  EXPECT_EQ(pkg.name, "com.test.app");
  EXPECT_EQ(pkg.debuggable, true);
  EXPECT_EQ(pkg.profileable_from_shell, true);
  EXPECT_EQ(pkg.version_code, 1111);
}

TEST(PackagesListDataSourceTest, EmptyNameFilterIncludesAll) {
  char buf[] =
      "com.test.one 1000 0 /data/user/0/com.test.one "
      "default:targetSdkVersion=10 none 0 10\n"
      "com.test.two 1001 0 /data/user/0/com.test.two "
      "default:targetSdkVersion=10 1065,3002 0 20\n"
      "com.test.three 1002 0 /data/user/0/com.test.three "
      "default:targetSdkVersion=10 1065,3002 0 30\n";

  // Create a stream from |buf|, up to the null byte. Avoid fmemopen as it
  // requires a higher target API (23) than we use for portability.
  auto pipe = base::Pipe::Create();
  PERFETTO_CHECK(write(pipe.wr.get(), buf, sizeof(buf) - 1) == sizeof(buf) - 1);
  pipe.wr.reset();
  auto fs = base::ScopedFstream(fdopen(pipe.rd.get(), "r"));
  pipe.rd.release();  // now owned by |fs|

  protozero::HeapBuffered<protos::pbzero::PackagesList> packages_list;
  std::set<std::string> filter{};

  ASSERT_TRUE(ParsePackagesListStream(packages_list.get(), fs, filter));

  protos::gen::PackagesList parsed_list;
  parsed_list.ParseFromString(packages_list.SerializeAsString());

  EXPECT_FALSE(parsed_list.read_error());
  EXPECT_FALSE(parsed_list.parse_error());
  // all entries
  EXPECT_EQ(parsed_list.packages_size(), 3);
  EXPECT_EQ(parsed_list.packages()[0].name(), "com.test.one");
  EXPECT_EQ(parsed_list.packages()[0].version_code(), 10);
  EXPECT_EQ(parsed_list.packages()[1].name(), "com.test.two");
  EXPECT_EQ(parsed_list.packages()[1].version_code(), 20);
  EXPECT_EQ(parsed_list.packages()[2].name(), "com.test.three");
  EXPECT_EQ(parsed_list.packages()[2].version_code(), 30);
}

TEST(PackagesListDataSourceTest, NameFilter) {
  char buf[] =
      "com.test.one 1000 0 /data/user/0/com.test.one "
      "default:targetSdkVersion=10 none 0 10\n"
      "com.test.two 1001 0 /data/user/0/com.test.two "
      "default:targetSdkVersion=10 1065,3002 0 20\n"
      "com.test.three 1002 0 /data/user/0/com.test.three "
      "default:targetSdkVersion=10 1065,3002 0 30\n";

  // Create a stream from |buf|, up to the null byte. Avoid fmemopen as it
  // requires a higher target API (23) than we use for portability.
  auto pipe = base::Pipe::Create();
  PERFETTO_CHECK(write(pipe.wr.get(), buf, sizeof(buf) - 1) == sizeof(buf) - 1);
  pipe.wr.reset();
  auto fs = base::ScopedFstream(fdopen(pipe.rd.get(), "r"));
  pipe.rd.release();  // now owned by |fs|

  protozero::HeapBuffered<protos::pbzero::PackagesList> packages_list;
  std::set<std::string> filter{"com.test.one", "com.test.three"};

  ASSERT_TRUE(ParsePackagesListStream(packages_list.get(), fs, filter));

  protos::gen::PackagesList parsed_list;
  parsed_list.ParseFromString(packages_list.SerializeAsString());

  EXPECT_FALSE(parsed_list.read_error());
  EXPECT_FALSE(parsed_list.parse_error());
  // two named entries
  EXPECT_EQ(parsed_list.packages_size(), 2);
  EXPECT_EQ(parsed_list.packages()[0].name(), "com.test.one");
  EXPECT_EQ(parsed_list.packages()[0].version_code(), 10);
  EXPECT_EQ(parsed_list.packages()[1].name(), "com.test.three");
  EXPECT_EQ(parsed_list.packages()[1].version_code(), 30);
}

}  // namespace
}  // namespace perfetto
