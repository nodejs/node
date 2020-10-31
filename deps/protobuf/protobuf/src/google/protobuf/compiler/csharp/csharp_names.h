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
//
// Provides a mechanism for mapping a descriptor to the
// fully-qualified name of the corresponding C# class.

#ifndef GOOGLE_PROTOBUF_COMPILER_CSHARP_NAMES_H__
#define GOOGLE_PROTOBUF_COMPILER_CSHARP_NAMES_H__

#include <string>
#include <google/protobuf/stubs/port.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

class Descriptor;
class EnumDescriptor;
class FileDescriptor;
class ServiceDescriptor;

namespace compiler {
namespace csharp {

// Requires:
//   descriptor != NULL
//
// Returns:
//   The namespace to use for given file descriptor.
string PROTOC_EXPORT GetFileNamespace(const FileDescriptor* descriptor);

// Requires:
//   descriptor != NULL
//
// Returns:
//   The fully-qualified C# class name.
string PROTOC_EXPORT GetClassName(const Descriptor* descriptor);

// Requires:
//   descriptor != NULL
//
// Returns:
//   The fully-qualified name of the C# class that provides
//   access to the file descriptor. Proto compiler generates
//   such class for each .proto file processed.
string PROTOC_EXPORT GetReflectionClassName(const FileDescriptor* descriptor);

// Generates output file name for given file descriptor. If generate_directories
// is true, the output file will be put under directory corresponding to file's
// namespace. base_namespace can be used to strip some of the top level
// directories. E.g. for file with namespace "Bar.Foo" and base_namespace="Bar",
// the resulting file will be put under directory "Foo" (and not "Bar/Foo").
//
// Requires:
//   descriptor != NULL
//   error != NULL
//
//  Returns:
//    The file name to use as output file for given file descriptor. In case
//    of failure, this function will return empty string and error parameter
//    will contain the error message.
string PROTOC_EXPORT GetOutputFile(const FileDescriptor* descriptor,
                                   const string file_extension,
                                   const bool generate_directories,
                                   const string base_namespace, string* error);

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_COMPILER_CSHARP_NAMES_H__
