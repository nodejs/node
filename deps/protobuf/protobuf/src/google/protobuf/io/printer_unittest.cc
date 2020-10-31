// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <vector>

#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace io {

// Each test repeats over several block sizes in order to test both cases
// where particular writes cross a buffer boundary and cases where they do
// not.

TEST(Printer, EmptyPrinter) {
  char buffer[8192];
  const int block_size = 100;
  ArrayOutputStream output(buffer, GOOGLE_ARRAYSIZE(buffer), block_size);
  Printer printer(&output, '\0');
  EXPECT_TRUE(!printer.failed());
}

TEST(Printer, BasicPrinting) {
  char buffer[8192];

  for (int block_size = 1; block_size < 512; block_size *= 2) {
    ArrayOutputStream output(buffer, sizeof(buffer), block_size);

    {
      Printer printer(&output, '\0');

      printer.Print("Hello World!");
      printer.Print("  This is the same line.\n");
      printer.Print("But this is a new one.\nAnd this is another one.");

      EXPECT_FALSE(printer.failed());
    }

    buffer[output.ByteCount()] = '\0';

    EXPECT_STREQ(
        "Hello World!  This is the same line.\n"
        "But this is a new one.\n"
        "And this is another one.",
        buffer);
  }
}

TEST(Printer, WriteRaw) {
  char buffer[8192];

  for (int block_size = 1; block_size < 512; block_size *= 2) {
    ArrayOutputStream output(buffer, sizeof(buffer), block_size);

    {
      std::string string_obj = "From an object\n";
      Printer printer(&output, '$');
      printer.WriteRaw("Hello World!", 12);
      printer.PrintRaw("  This is the same line.\n");
      printer.PrintRaw("But this is a new one.\nAnd this is another one.");
      printer.WriteRaw("\n", 1);
      printer.PrintRaw(string_obj);
      EXPECT_FALSE(printer.failed());
    }

    buffer[output.ByteCount()] = '\0';

    EXPECT_STREQ(
        "Hello World!  This is the same line.\n"
        "But this is a new one.\n"
        "And this is another one."
        "\n"
        "From an object\n",
        buffer);
  }
}

TEST(Printer, VariableSubstitution) {
  char buffer[8192];

  for (int block_size = 1; block_size < 512; block_size *= 2) {
    ArrayOutputStream output(buffer, sizeof(buffer), block_size);

    {
      Printer printer(&output, '$');
      std::map<std::string, std::string> vars;

      vars["foo"] = "World";
      vars["bar"] = "$foo$";
      vars["abcdefg"] = "1234";

      printer.Print(vars, "Hello $foo$!\nbar = $bar$\n");
      printer.PrintRaw("RawBit\n");
      printer.Print(vars, "$abcdefg$\nA literal dollar sign:  $$");

      vars["foo"] = "blah";
      printer.Print(vars, "\nNow foo = $foo$.");

      EXPECT_FALSE(printer.failed());
    }

    buffer[output.ByteCount()] = '\0';

    EXPECT_STREQ(
        "Hello World!\n"
        "bar = $foo$\n"
        "RawBit\n"
        "1234\n"
        "A literal dollar sign:  $\n"
        "Now foo = blah.",
        buffer);
  }
}

TEST(Printer, InlineVariableSubstitution) {
  char buffer[8192];

  ArrayOutputStream output(buffer, sizeof(buffer));

  {
    Printer printer(&output, '$');
    printer.Print("Hello $foo$!\n", "foo", "World");
    printer.PrintRaw("RawBit\n");
    printer.Print("$foo$ $bar$\n", "foo", "one", "bar", "two");
    EXPECT_FALSE(printer.failed());
  }

  buffer[output.ByteCount()] = '\0';

  EXPECT_STREQ(
      "Hello World!\n"
      "RawBit\n"
      "one two\n",
      buffer);
}

// MockDescriptorFile defines only those members that Printer uses to write out
// annotations.
class MockDescriptorFile {
 public:
  explicit MockDescriptorFile(const std::string& file) : file_(file) {}

  // The mock filename for this file.
  const std::string& name() const { return file_; }

 private:
  std::string file_;
};

// MockDescriptor defines only those members that Printer uses to write out
// annotations.
class MockDescriptor {
 public:
  MockDescriptor(const std::string& file, const std::vector<int>& path)
      : file_(file), path_(path) {}

  // The mock file in which this descriptor was defined.
  const MockDescriptorFile* file() const { return &file_; }

 private:
  // Allows access to GetLocationPath.
  friend class Printer;

  // Copies the pre-stored path to output.
  void GetLocationPath(std::vector<int>* output) const { *output = path_; }

  MockDescriptorFile file_;
  std::vector<int> path_;
};

TEST(Printer, AnnotateMap) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    std::map<std::string, std::string> vars;
    vars["foo"] = "3";
    vars["bar"] = "5";
    printer.Print(vars, "012$foo$4$bar$\n");
    std::vector<int> path_1;
    path_1.push_back(33);
    std::vector<int> path_2;
    path_2.push_back(11);
    path_2.push_back(22);
    MockDescriptor descriptor_1("path_1", path_1);
    MockDescriptor descriptor_2("path_2", path_2);
    printer.Annotate("foo", "foo", &descriptor_1);
    printer.Annotate("bar", "bar", &descriptor_2);
  }
  buffer[output.ByteCount()] = '\0';
  EXPECT_STREQ("012345\n", buffer);
  ASSERT_EQ(2, info.annotation_size());
  const GeneratedCodeInfo::Annotation* foo = info.annotation(0).path_size() == 1
                                                 ? &info.annotation(0)
                                                 : &info.annotation(1);
  const GeneratedCodeInfo::Annotation* bar = info.annotation(0).path_size() == 1
                                                 ? &info.annotation(1)
                                                 : &info.annotation(0);
  ASSERT_EQ(1, foo->path_size());
  ASSERT_EQ(2, bar->path_size());
  EXPECT_EQ(33, foo->path(0));
  EXPECT_EQ(11, bar->path(0));
  EXPECT_EQ(22, bar->path(1));
  EXPECT_EQ("path_1", foo->source_file());
  EXPECT_EQ("path_2", bar->source_file());
  EXPECT_EQ(3, foo->begin());
  EXPECT_EQ(4, foo->end());
  EXPECT_EQ(5, bar->begin());
  EXPECT_EQ(6, bar->end());
}

TEST(Printer, AnnotateInline) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Print("012$foo$4$bar$\n", "foo", "3", "bar", "5");
    std::vector<int> path_1;
    path_1.push_back(33);
    std::vector<int> path_2;
    path_2.push_back(11);
    path_2.push_back(22);
    MockDescriptor descriptor_1("path_1", path_1);
    MockDescriptor descriptor_2("path_2", path_2);
    printer.Annotate("foo", "foo", &descriptor_1);
    printer.Annotate("bar", "bar", &descriptor_2);
  }
  buffer[output.ByteCount()] = '\0';
  EXPECT_STREQ("012345\n", buffer);
  ASSERT_EQ(2, info.annotation_size());
  const GeneratedCodeInfo::Annotation* foo = info.annotation(0).path_size() == 1
                                                 ? &info.annotation(0)
                                                 : &info.annotation(1);
  const GeneratedCodeInfo::Annotation* bar = info.annotation(0).path_size() == 1
                                                 ? &info.annotation(1)
                                                 : &info.annotation(0);
  ASSERT_EQ(1, foo->path_size());
  ASSERT_EQ(2, bar->path_size());
  EXPECT_EQ(33, foo->path(0));
  EXPECT_EQ(11, bar->path(0));
  EXPECT_EQ(22, bar->path(1));
  EXPECT_EQ("path_1", foo->source_file());
  EXPECT_EQ("path_2", bar->source_file());
  EXPECT_EQ(3, foo->begin());
  EXPECT_EQ(4, foo->end());
  EXPECT_EQ(5, bar->begin());
  EXPECT_EQ(6, bar->end());
}

TEST(Printer, AnnotateRange) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Print("012$foo$4$bar$\n", "foo", "3", "bar", "5");
    std::vector<int> path;
    path.push_back(33);
    MockDescriptor descriptor("path", path);
    printer.Annotate("foo", "bar", &descriptor);
  }
  buffer[output.ByteCount()] = '\0';
  EXPECT_STREQ("012345\n", buffer);
  ASSERT_EQ(1, info.annotation_size());
  const GeneratedCodeInfo::Annotation* foobar = &info.annotation(0);
  ASSERT_EQ(1, foobar->path_size());
  EXPECT_EQ(33, foobar->path(0));
  EXPECT_EQ("path", foobar->source_file());
  EXPECT_EQ(3, foobar->begin());
  EXPECT_EQ(6, foobar->end());
}

TEST(Printer, AnnotateEmptyRange) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Print("012$foo$4$baz$$bam$$bar$\n", "foo", "3", "bar", "5", "baz",
                  "", "bam", "");
    std::vector<int> path;
    path.push_back(33);
    MockDescriptor descriptor("path", path);
    printer.Annotate("baz", "bam", &descriptor);
  }
  buffer[output.ByteCount()] = '\0';
  EXPECT_STREQ("012345\n", buffer);
  ASSERT_EQ(1, info.annotation_size());
  const GeneratedCodeInfo::Annotation* bazbam = &info.annotation(0);
  ASSERT_EQ(1, bazbam->path_size());
  EXPECT_EQ(33, bazbam->path(0));
  EXPECT_EQ("path", bazbam->source_file());
  EXPECT_EQ(5, bazbam->begin());
  EXPECT_EQ(5, bazbam->end());
}

TEST(Printer, AnnotateDespiteUnrelatedMultipleUses) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Print("012$foo$4$foo$$bar$\n", "foo", "3", "bar", "5");
    std::vector<int> path;
    path.push_back(33);
    MockDescriptor descriptor("path", path);
    printer.Annotate("bar", "bar", &descriptor);
  }
  buffer[output.ByteCount()] = '\0';
  EXPECT_STREQ("0123435\n", buffer);
  ASSERT_EQ(1, info.annotation_size());
  const GeneratedCodeInfo::Annotation* bar = &info.annotation(0);
  ASSERT_EQ(1, bar->path_size());
  EXPECT_EQ(33, bar->path(0));
  EXPECT_EQ("path", bar->source_file());
  EXPECT_EQ(6, bar->begin());
  EXPECT_EQ(7, bar->end());
}

TEST(Printer, AnnotateIndent) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Print("0\n");
    printer.Indent();
    printer.Print("$foo$", "foo", "4");
    std::vector<int> path;
    path.push_back(44);
    MockDescriptor descriptor("path", path);
    printer.Annotate("foo", &descriptor);
    printer.Print(",\n");
    printer.Print("$bar$", "bar", "9");
    path[0] = 99;
    MockDescriptor descriptor_two("path", path);
    printer.Annotate("bar", &descriptor_two);
    printer.Print("\n${$$D$$}$\n", "{", "", "}", "", "D", "d");
    path[0] = 1313;
    MockDescriptor descriptor_three("path", path);
    printer.Annotate("{", "}", &descriptor_three);
    printer.Outdent();
    printer.Print("\n");
  }
  buffer[output.ByteCount()] = '\0';
  EXPECT_STREQ("0\n  4,\n  9\n  d\n\n", buffer);
  ASSERT_EQ(3, info.annotation_size());
  const GeneratedCodeInfo::Annotation* foo = &info.annotation(0);
  ASSERT_EQ(1, foo->path_size());
  EXPECT_EQ(44, foo->path(0));
  EXPECT_EQ("path", foo->source_file());
  EXPECT_EQ(4, foo->begin());
  EXPECT_EQ(5, foo->end());
  const GeneratedCodeInfo::Annotation* bar = &info.annotation(1);
  ASSERT_EQ(1, bar->path_size());
  EXPECT_EQ(99, bar->path(0));
  EXPECT_EQ("path", bar->source_file());
  EXPECT_EQ(9, bar->begin());
  EXPECT_EQ(10, bar->end());
  const GeneratedCodeInfo::Annotation* braces = &info.annotation(2);
  ASSERT_EQ(1, braces->path_size());
  EXPECT_EQ(1313, braces->path(0));
  EXPECT_EQ("path", braces->source_file());
  EXPECT_EQ(13, braces->begin());
  EXPECT_EQ(14, braces->end());
}

TEST(Printer, AnnotateIndentNewline) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Indent();
    printer.Print("$A$$N$$B$C\n", "A", "", "N", "\nz", "B", "");
    std::vector<int> path;
    path.push_back(0);
    MockDescriptor descriptor("path", path);
    printer.Annotate("A", "B", &descriptor);
    printer.Outdent();
    printer.Print("\n");
  }
  buffer[output.ByteCount()] = '\0';
  EXPECT_STREQ("\nz  C\n\n", buffer);
  ASSERT_EQ(1, info.annotation_size());
  const GeneratedCodeInfo::Annotation* ab = &info.annotation(0);
  ASSERT_EQ(1, ab->path_size());
  EXPECT_EQ(0, ab->path(0));
  EXPECT_EQ("path", ab->source_file());
  EXPECT_EQ(0, ab->begin());
  EXPECT_EQ(4, ab->end());
}

TEST(Printer, Indenting) {
  char buffer[8192];

  for (int block_size = 1; block_size < 512; block_size *= 2) {
    ArrayOutputStream output(buffer, sizeof(buffer), block_size);

    {
      Printer printer(&output, '$');
      std::map<std::string, std::string> vars;

      vars["newline"] = "\n";

      printer.Print("This is not indented.\n");
      printer.Indent();
      printer.Print("This is indented\nAnd so is this\n");
      printer.Outdent();
      printer.Print("But this is not.");
      printer.Indent();
      printer.Print(
          "  And this is still the same line.\n"
          "But this is indented.\n");
      printer.PrintRaw("RawBit has indent at start\n");
      printer.PrintRaw("but not after a raw newline\n");
      printer.Print(vars,
                    "Note that a newline in a variable will break "
                    "indenting, as we see$newline$here.\n");
      printer.Indent();
      printer.Print("And this");
      printer.Outdent();
      printer.Outdent();
      printer.Print(" is double-indented\nBack to normal.");

      EXPECT_FALSE(printer.failed());
    }

    buffer[output.ByteCount()] = '\0';

    EXPECT_STREQ(
        "This is not indented.\n"
        "  This is indented\n"
        "  And so is this\n"
        "But this is not.  And this is still the same line.\n"
        "  But this is indented.\n"
        "  RawBit has indent at start\n"
        "but not after a raw newline\n"
        "Note that a newline in a variable will break indenting, as we see\n"
        "here.\n"
        "    And this is double-indented\n"
        "Back to normal.",
        buffer);
  }
}

// Death tests do not work on Windows as of yet.
#ifdef PROTOBUF_HAS_DEATH_TEST
TEST(Printer, Death) {
  char buffer[8192];

  ArrayOutputStream output(buffer, sizeof(buffer));
  Printer printer(&output, '$');

  EXPECT_DEBUG_DEATH(printer.Print("$nosuchvar$"), "Undefined variable");
  EXPECT_DEBUG_DEATH(printer.Print("$unclosed"), "Unclosed variable name");
  EXPECT_DEBUG_DEATH(printer.Outdent(), "without matching Indent");
}

TEST(Printer, AnnotateMultipleUsesDeath) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Print("012$foo$4$foo$\n", "foo", "3");
    std::vector<int> path;
    path.push_back(33);
    MockDescriptor descriptor("path", path);
    EXPECT_DEBUG_DEATH(printer.Annotate("foo", "foo", &descriptor), "multiple");
  }
}

TEST(Printer, AnnotateNegativeLengthDeath) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Print("012$foo$4$bar$\n", "foo", "3", "bar", "5");
    std::vector<int> path;
    path.push_back(33);
    MockDescriptor descriptor("path", path);
    EXPECT_DEBUG_DEATH(printer.Annotate("bar", "foo", &descriptor), "negative");
  }
}

TEST(Printer, AnnotateUndefinedDeath) {
  char buffer[8192];
  ArrayOutputStream output(buffer, sizeof(buffer));
  GeneratedCodeInfo info;
  AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
  {
    Printer printer(&output, '$', &info_collector);
    printer.Print("012$foo$4$foo$\n", "foo", "3");
    std::vector<int> path;
    path.push_back(33);
    MockDescriptor descriptor("path", path);
    EXPECT_DEBUG_DEATH(printer.Annotate("bar", "bar", &descriptor),
                       "Undefined");
  }
}
#endif  // PROTOBUF_HAS_DEATH_TEST

TEST(Printer, WriteFailurePartial) {
  char buffer[17];

  ArrayOutputStream output(buffer, sizeof(buffer));
  Printer printer(&output, '$');

  // Print 16 bytes to almost fill the buffer (should not fail).
  printer.Print("0123456789abcdef");
  EXPECT_FALSE(printer.failed());

  // Try to print 2 chars. Only one fits.
  printer.Print("<>");
  EXPECT_TRUE(printer.failed());

  // Anything else should fail too.
  printer.Print(" ");
  EXPECT_TRUE(printer.failed());
  printer.Print("blah");
  EXPECT_TRUE(printer.failed());

  // Buffer should contain the first 17 bytes written.
  EXPECT_EQ("0123456789abcdef<", string(buffer, sizeof(buffer)));
}

TEST(Printer, WriteFailureExact) {
  char buffer[16];

  ArrayOutputStream output(buffer, sizeof(buffer));
  Printer printer(&output, '$');

  // Print 16 bytes to fill the buffer exactly (should not fail).
  printer.Print("0123456789abcdef");
  EXPECT_FALSE(printer.failed());

  // Try to print one more byte (should fail).
  printer.Print(" ");
  EXPECT_TRUE(printer.failed());

  // Should not crash
  printer.Print("blah");
  EXPECT_TRUE(printer.failed());

  // Buffer should contain the first 16 bytes written.
  EXPECT_EQ("0123456789abcdef", string(buffer, sizeof(buffer)));
}

TEST(Printer, FormatInternal) {
  std::vector<std::string> args{"arg1", "arg2"};
  std::map<std::string, std::string> vars{
      {"foo", "bar"}, {"baz", "bla"}, {"empty", ""}};
  // Substitution tests
  {
    // Direct arg substitution
    std::string s;
    {
      StringOutputStream output(&s);
      Printer printer(&output, '$');
      printer.FormatInternal(args, vars, "$1$ $2$");
    }
    EXPECT_EQ("arg1 arg2", s);
  }
  {
    // Variable substitution including spaces left
    std::string s;
    {
      StringOutputStream output(&s);
      Printer printer(&output, '$');
      printer.FormatInternal({}, vars, "$foo$$ baz$$ empty$");
    }
    EXPECT_EQ("bar bla", s);
  }
  {
    // Variable substitution including spaces right
    std::string s;
    {
      StringOutputStream output(&s);
      Printer printer(&output, '$');
      printer.FormatInternal({}, vars, "$empty $$foo $$baz$");
    }
    EXPECT_EQ("bar bla", s);
  }
  {
    // Mixed variable substitution
    std::string s;
    {
      StringOutputStream output(&s);
      Printer printer(&output, '$');
      printer.FormatInternal(args, vars, "$empty $$1$ $foo $$2$ $baz$");
    }
    EXPECT_EQ("arg1 bar arg2 bla", s);
  }

  // Indentation tests
  {
    // Empty lines shouldn't indent.
    std::string s;
    {
      StringOutputStream output(&s);
      Printer printer(&output, '$');
      printer.Indent();
      printer.FormatInternal(args, vars, "$empty $\n\n$1$ $foo $$2$\n$baz$");
      printer.Outdent();
    }
    EXPECT_EQ("\n\n  arg1 bar arg2\n  bla", s);
  }
  {
    // Annotations should respect indentation.
    std::string s;
    GeneratedCodeInfo info;
    {
      StringOutputStream output(&s);
      AnnotationProtoCollector<GeneratedCodeInfo> info_collector(&info);
      Printer printer(&output, '$', &info_collector);
      printer.Indent();
      GeneratedCodeInfo::Annotation annotation;
      annotation.set_source_file("file.proto");
      annotation.add_path(33);
      std::vector<std::string> args{annotation.SerializeAsString(), "arg1",
                                    "arg2"};
      printer.FormatInternal(args, vars, "$empty $\n\n${1$$2$$}$ $3$\n$baz$");
      printer.Outdent();
    }
    EXPECT_EQ("\n\n  arg1 arg2\n  bla", s);
    ASSERT_EQ(1, info.annotation_size());
    const GeneratedCodeInfo::Annotation* arg1 = &info.annotation(0);
    ASSERT_EQ(1, arg1->path_size());
    EXPECT_EQ(33, arg1->path(0));
    EXPECT_EQ("file.proto", arg1->source_file());
    EXPECT_EQ(4, arg1->begin());
    EXPECT_EQ(8, arg1->end());
  }
#ifdef PROTOBUF_HAS_DEATH_TEST
  // Death tests in case of illegal format strings.
  {
    // Unused arguments
    std::string s;
    StringOutputStream output(&s);
    Printer printer(&output, '$');
    EXPECT_DEATH(printer.FormatInternal(args, vars, "$empty $$1$"), "Unused");
  }
  {
    // Wrong order arguments
    std::string s;
    StringOutputStream output(&s);
    Printer printer(&output, '$');
    EXPECT_DEATH(printer.FormatInternal(args, vars, "$2$ $1$"), "order");
  }
  {
    // Zero is illegal argument
    std::string s;
    StringOutputStream output(&s);
    Printer printer(&output, '$');
    EXPECT_DEATH(printer.FormatInternal(args, vars, "$0$"), "failed");
  }
  {
    // Argument out of bounds
    std::string s;
    StringOutputStream output(&s);
    Printer printer(&output, '$');
    EXPECT_DEATH(printer.FormatInternal(args, vars, "$1$ $2$ $3$"), "bounds");
  }
  {
    // Unknown variable
    std::string s;
    StringOutputStream output(&s);
    Printer printer(&output, '$');
    EXPECT_DEATH(printer.FormatInternal(args, vars, "$huh$ $1$$2$"), "Unknown");
  }
  {
    // Illegal variable
    std::string s;
    StringOutputStream output(&s);
    Printer printer(&output, '$');
    EXPECT_DEATH(printer.FormatInternal({}, vars, "$ $"), "Empty");
  }
#endif  // PROTOBUF_HAS_DEATH_TEST
}

}  // namespace io
}  // namespace protobuf
}  // namespace google
