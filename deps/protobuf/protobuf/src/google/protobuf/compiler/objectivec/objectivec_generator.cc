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

#include <iostream>
#include <google/protobuf/compiler/objectivec/objectivec_generator.h>
#include <google/protobuf/compiler/objectivec/objectivec_file.h>
#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

ObjectiveCGenerator::ObjectiveCGenerator() {}

ObjectiveCGenerator::~ObjectiveCGenerator() {}

bool ObjectiveCGenerator::HasGenerateAll() const {
  return true;
}

bool ObjectiveCGenerator::Generate(const FileDescriptor* file,
                                   const string& parameter,
                                   GeneratorContext* context,
                                   string* error) const {
  *error = "Unimplemented Generate() method. Call GenerateAll() instead.";
  return false;
}

bool ObjectiveCGenerator::GenerateAll(const std::vector<const FileDescriptor*>& files,
                                      const string& parameter,
                                      GeneratorContext* context,
                                      string* error) const {
  // -----------------------------------------------------------------
  // Parse generator options. These options are passed to the compiler using the
  // --objc_opt flag. The options are passed as a comma separated list of
  // options along with their values. If the option appears multiple times, only
  // the last value will be considered.
  //
  // e.g. protoc ... --objc_opt=expected_prefixes=file.txt,generate_for_named_framework=MyFramework

  Options generation_options;

  std::vector<std::pair<string, string> > options;
  ParseGeneratorParameter(parameter, &options);
  for (int i = 0; i < options.size(); i++) {
    if (options[i].first == "expected_prefixes_path") {
      // Path to find a file containing the expected prefixes
      // (objc_class_prefix "PREFIX") for proto packages (package NAME). The
      // generator will then issue warnings/errors if in the proto files being
      // generated the option is not listed/wrong/etc in the file.
      //
      // The format of the file is:
      //   - An entry is a line of "package=prefix".
      //   - Comments start with "#".
      //   - A comment can go on a line after a expected package/prefix pair.
      //     (i.e. - "package=prefix # comment")
      //
      // There is no validation that the prefixes are good prefixes, it is
      // assumed that they are when you create the file.
      generation_options.expected_prefixes_path = options[i].second;
    } else if (options[i].first == "expected_prefixes_suppressions") {
      // A semicolon delimited string that lists the paths of .proto files to
      // exclude from the package prefix validations (expected_prefixes_path).
      // This is provided as an "out", to skip some files being checked.
      SplitStringUsing(options[i].second, ";",
                       &generation_options.expected_prefixes_suppressions);
    } else if (options[i].first == "generate_for_named_framework") {
      // The name of the framework that protos are being generated for. This
      // will cause the #import statements to be framework based using this
      // name (i.e. - "#import <NAME/proto.pbobjc.h>).
      //
      // NOTE: If this option is used with
      // named_framework_to_proto_path_mappings_path, then this is effectively
      // the "default" framework name used for everything that wasn't mapped by
      // the mapping file.
      generation_options.generate_for_named_framework = options[i].second;
    } else if (options[i].first == "named_framework_to_proto_path_mappings_path") {
      // Path to find a file containing the list of framework names and proto
      // files. The generator uses this to decide if a proto file
      // referenced should use a framework style import vs. a user level import
      // (#import <FRAMEWORK/file.pbobjc.h> vs #import "dir/file.pbobjc.h").
      //
      // The format of the file is:
      //   - An entry is a line of "frameworkName: file.proto, dir/file2.proto".
      //   - Comments start with "#".
      //   - A comment can go on a line after a expected package/prefix pair.
      //     (i.e. - "frameworkName: file.proto # comment")
      //
      // Any number of files can be listed for a framework, just separate them
      // with commas.
      //
      // There can be multiple lines listing the same frameworkName incase it
      // has a lot of proto files included in it; having multiple lines makes
      // things easier to read. If a proto file is not configured in the
      // mappings file, it will use the default framework name if one was passed
      // with generate_for_named_framework, or the relative path to it's include
      // path otherwise.
      generation_options.named_framework_to_proto_path_mappings_path = options[i].second;
    } else {
      *error = "error: Unknown generator option: " + options[i].first;
      return false;
    }
  }

  // -----------------------------------------------------------------

  // Validate the objc prefix/package pairings.
  if (!ValidateObjCClassPrefixes(files, generation_options, error)) {
    // *error will have been filled in.
    return false;
  }

  for (int i = 0; i < files.size(); i++) {
    const FileDescriptor* file = files[i];
    FileGenerator file_generator(file, generation_options);
    string filepath = FilePath(file);

    // Generate header.
    {
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          context->Open(filepath + ".pbobjc.h"));
      io::Printer printer(output.get(), '$');
      file_generator.GenerateHeader(&printer);
    }

    // Generate m file.
    {
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          context->Open(filepath + ".pbobjc.m"));
      io::Printer printer(output.get(), '$');
      file_generator.GenerateSource(&printer);
    }
  }

  return true;
}

}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
