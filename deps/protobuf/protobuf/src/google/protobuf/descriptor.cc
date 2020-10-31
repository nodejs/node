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

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/strtod.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/stubs/casts.h>



#include <google/protobuf/stubs/map_util.h>
#include <google/protobuf/stubs/stl_util.h>
#include <google/protobuf/stubs/hash.h>

#undef PACKAGE  // autoheader #defines this.  :(


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

struct Symbol {
  enum Type {
    NULL_SYMBOL,
    MESSAGE,
    FIELD,
    ONEOF,
    ENUM,
    ENUM_VALUE,
    SERVICE,
    METHOD,
    PACKAGE
  };
  Type type;
  union {
    const Descriptor* descriptor;
    const FieldDescriptor* field_descriptor;
    const OneofDescriptor* oneof_descriptor;
    const EnumDescriptor* enum_descriptor;
    const EnumValueDescriptor* enum_value_descriptor;
    const ServiceDescriptor* service_descriptor;
    const MethodDescriptor* method_descriptor;
    const FileDescriptor* package_file_descriptor;
  };

  inline Symbol() : type(NULL_SYMBOL) { descriptor = nullptr; }
  inline bool IsNull() const { return type == NULL_SYMBOL; }
  inline bool IsType() const { return type == MESSAGE || type == ENUM; }
  inline bool IsAggregate() const {
    return type == MESSAGE || type == PACKAGE || type == ENUM ||
           type == SERVICE;
  }

#define CONSTRUCTOR(TYPE, TYPE_CONSTANT, FIELD) \
  inline explicit Symbol(const TYPE* value) {   \
    type = TYPE_CONSTANT;                       \
    this->FIELD = value;                        \
  }

  CONSTRUCTOR(Descriptor, MESSAGE, descriptor)
  CONSTRUCTOR(FieldDescriptor, FIELD, field_descriptor)
  CONSTRUCTOR(OneofDescriptor, ONEOF, oneof_descriptor)
  CONSTRUCTOR(EnumDescriptor, ENUM, enum_descriptor)
  CONSTRUCTOR(EnumValueDescriptor, ENUM_VALUE, enum_value_descriptor)
  CONSTRUCTOR(ServiceDescriptor, SERVICE, service_descriptor)
  CONSTRUCTOR(MethodDescriptor, METHOD, method_descriptor)
  CONSTRUCTOR(FileDescriptor, PACKAGE, package_file_descriptor)
#undef CONSTRUCTOR

  const FileDescriptor* GetFile() const {
    switch (type) {
      case NULL_SYMBOL:
        return nullptr;
      case MESSAGE:
        return descriptor->file();
      case FIELD:
        return field_descriptor->file();
      case ONEOF:
        return oneof_descriptor->containing_type()->file();
      case ENUM:
        return enum_descriptor->file();
      case ENUM_VALUE:
        return enum_value_descriptor->type()->file();
      case SERVICE:
        return service_descriptor->file();
      case METHOD:
        return method_descriptor->service()->file();
      case PACKAGE:
        return package_file_descriptor;
    }
    return nullptr;
  }
};

const FieldDescriptor::CppType
    FieldDescriptor::kTypeToCppTypeMap[MAX_TYPE + 1] = {
        static_cast<CppType>(0),  // 0 is reserved for errors

        CPPTYPE_DOUBLE,   // TYPE_DOUBLE
        CPPTYPE_FLOAT,    // TYPE_FLOAT
        CPPTYPE_INT64,    // TYPE_INT64
        CPPTYPE_UINT64,   // TYPE_UINT64
        CPPTYPE_INT32,    // TYPE_INT32
        CPPTYPE_UINT64,   // TYPE_FIXED64
        CPPTYPE_UINT32,   // TYPE_FIXED32
        CPPTYPE_BOOL,     // TYPE_BOOL
        CPPTYPE_STRING,   // TYPE_STRING
        CPPTYPE_MESSAGE,  // TYPE_GROUP
        CPPTYPE_MESSAGE,  // TYPE_MESSAGE
        CPPTYPE_STRING,   // TYPE_BYTES
        CPPTYPE_UINT32,   // TYPE_UINT32
        CPPTYPE_ENUM,     // TYPE_ENUM
        CPPTYPE_INT32,    // TYPE_SFIXED32
        CPPTYPE_INT64,    // TYPE_SFIXED64
        CPPTYPE_INT32,    // TYPE_SINT32
        CPPTYPE_INT64,    // TYPE_SINT64
};

const char* const FieldDescriptor::kTypeToName[MAX_TYPE + 1] = {
    "ERROR",  // 0 is reserved for errors

    "double",    // TYPE_DOUBLE
    "float",     // TYPE_FLOAT
    "int64",     // TYPE_INT64
    "uint64",    // TYPE_UINT64
    "int32",     // TYPE_INT32
    "fixed64",   // TYPE_FIXED64
    "fixed32",   // TYPE_FIXED32
    "bool",      // TYPE_BOOL
    "string",    // TYPE_STRING
    "group",     // TYPE_GROUP
    "message",   // TYPE_MESSAGE
    "bytes",     // TYPE_BYTES
    "uint32",    // TYPE_UINT32
    "enum",      // TYPE_ENUM
    "sfixed32",  // TYPE_SFIXED32
    "sfixed64",  // TYPE_SFIXED64
    "sint32",    // TYPE_SINT32
    "sint64",    // TYPE_SINT64
};

const char* const FieldDescriptor::kCppTypeToName[MAX_CPPTYPE + 1] = {
    "ERROR",  // 0 is reserved for errors

    "int32",    // CPPTYPE_INT32
    "int64",    // CPPTYPE_INT64
    "uint32",   // CPPTYPE_UINT32
    "uint64",   // CPPTYPE_UINT64
    "double",   // CPPTYPE_DOUBLE
    "float",    // CPPTYPE_FLOAT
    "bool",     // CPPTYPE_BOOL
    "enum",     // CPPTYPE_ENUM
    "string",   // CPPTYPE_STRING
    "message",  // CPPTYPE_MESSAGE
};

const char* const FieldDescriptor::kLabelToName[MAX_LABEL + 1] = {
    "ERROR",  // 0 is reserved for errors

    "optional",  // LABEL_OPTIONAL
    "required",  // LABEL_REQUIRED
    "repeated",  // LABEL_REPEATED
};

const char* FileDescriptor::SyntaxName(FileDescriptor::Syntax syntax) {
  switch (syntax) {
    case SYNTAX_PROTO2:
      return "proto2";
    case SYNTAX_PROTO3:
      return "proto3";
    case SYNTAX_UNKNOWN:
      return "unknown";
  }
  GOOGLE_LOG(FATAL) << "can't reach here.";
  return nullptr;
}

static const char* const kNonLinkedWeakMessageReplacementName = "google.protobuf.Empty";

#if !defined(_MSC_VER) || _MSC_VER >= 1900
const int FieldDescriptor::kMaxNumber;
const int FieldDescriptor::kFirstReservedNumber;
const int FieldDescriptor::kLastReservedNumber;
#endif

namespace {

// Note:  I distrust ctype.h due to locales.
char ToUpper(char ch) {
  return (ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch;
}

char ToLower(char ch) {
  return (ch >= 'A' && ch <= 'Z') ? (ch - 'A' + 'a') : ch;
}

std::string ToCamelCase(const std::string& input, bool lower_first) {
  bool capitalize_next = !lower_first;
  std::string result;
  result.reserve(input.size());

  for (int i = 0; i < input.size(); i++) {
    if (input[i] == '_') {
      capitalize_next = true;
    } else if (capitalize_next) {
      result.push_back(ToUpper(input[i]));
      capitalize_next = false;
    } else {
      result.push_back(input[i]);
    }
  }

  // Lower-case the first letter.
  if (lower_first && !result.empty()) {
    result[0] = ToLower(result[0]);
  }

  return result;
}

std::string ToJsonName(const std::string& input) {
  bool capitalize_next = false;
  std::string result;
  result.reserve(input.size());

  for (int i = 0; i < input.size(); i++) {
    if (input[i] == '_') {
      capitalize_next = true;
    } else if (capitalize_next) {
      result.push_back(ToUpper(input[i]));
      capitalize_next = false;
    } else {
      result.push_back(input[i]);
    }
  }

  return result;
}

std::string EnumValueToPascalCase(const std::string& input) {
  bool next_upper = true;
  std::string result;
  result.reserve(input.size());

  for (int i = 0; i < input.size(); i++) {
    if (input[i] == '_') {
      next_upper = true;
    } else {
      if (next_upper) {
        result.push_back(ToUpper(input[i]));
      } else {
        result.push_back(ToLower(input[i]));
      }
      next_upper = false;
    }
  }

  return result;
}

// Class to remove an enum prefix from enum values.
class PrefixRemover {
 public:
  PrefixRemover(StringPiece prefix) {
    // Strip underscores and lower-case the prefix.
    for (int i = 0; i < prefix.size(); i++) {
      if (prefix[i] != '_') {
        prefix_ += ascii_tolower(prefix[i]);
      }
    }
  }

  // Tries to remove the enum prefix from this enum value.
  // If this is not possible, returns the input verbatim.
  std::string MaybeRemove(StringPiece str) {
    // We can't just lowercase and strip str and look for a prefix.
    // We need to properly recognize the difference between:
    //
    //   enum Foo {
    //     FOO_BAR_BAZ = 0;
    //     FOO_BARBAZ = 1;
    //   }
    //
    // This is acceptable (though perhaps not advisable) because even when
    // we PascalCase, these two will still be distinct (BarBaz vs. Barbaz).
    size_t i, j;

    // Skip past prefix_ in str if we can.
    for (i = 0, j = 0; i < str.size() && j < prefix_.size(); i++) {
      if (str[i] == '_') {
        continue;
      }

      if (ascii_tolower(str[i]) != prefix_[j++]) {
        return std::string(str);
      }
    }

    // If we didn't make it through the prefix, we've failed to strip the
    // prefix.
    if (j < prefix_.size()) {
      return std::string(str);
    }

    // Skip underscores between prefix and further characters.
    while (i < str.size() && str[i] == '_') {
      i++;
    }

    // Enum label can't be the empty string.
    if (i == str.size()) {
      return std::string(str);
    }

    // We successfully stripped the prefix.
    str.remove_prefix(i);
    return std::string(str);
  }

 private:
  std::string prefix_;
};

// A DescriptorPool contains a bunch of hash-maps to implement the
// various Find*By*() methods.  Since hashtable lookups are O(1), it's
// most efficient to construct a fixed set of large hash-maps used by
// all objects in the pool rather than construct one or more small
// hash-maps for each object.
//
// The keys to these hash-maps are (parent, name) or (parent, number) pairs.
//
// TODO(kenton):  Use StringPiece rather than const char* in keys?  It would
//   be a lot cleaner but we'd just have to convert it back to const char*
//   for the open source release.

typedef std::pair<const void*, const char*> PointerStringPair;

struct PointerStringPairEqual {
  inline bool operator()(const PointerStringPair& a,
                         const PointerStringPair& b) const {
    return a.first == b.first && strcmp(a.second, b.second) == 0;
  }
};

typedef std::pair<const Descriptor*, int> DescriptorIntPair;
typedef std::pair<const EnumDescriptor*, int> EnumIntPair;

#define HASH_MAP std::unordered_map
#define HASH_SET std::unordered_set
#define HASH_FXN hash

template <typename PairType>
struct PointerIntegerPairHash {
  size_t operator()(const PairType& p) const {
    static const size_t prime1 = 16777499;
    static const size_t prime2 = 16777619;
    return reinterpret_cast<size_t>(p.first) * prime1 ^
           static_cast<size_t>(p.second) * prime2;
  }

#ifdef _MSC_VER
  // Used only by MSVC and platforms where hash_map is not available.
  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
#endif
  inline bool operator()(const PairType& a, const PairType& b) const {
    return a.first < b.first || (a.first == b.first && a.second < b.second);
  }
};

struct PointerStringPairHash {
  size_t operator()(const PointerStringPair& p) const {
    static const size_t prime = 16777619;
    hash<const char*> cstring_hash;
    return reinterpret_cast<size_t>(p.first) * prime ^
           static_cast<size_t>(cstring_hash(p.second));
  }

#ifdef _MSC_VER
  // Used only by MSVC and platforms where hash_map is not available.
  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
#endif
  inline bool operator()(const PointerStringPair& a,
                         const PointerStringPair& b) const {
    if (a.first < b.first) return true;
    if (a.first > b.first) return false;
    return strcmp(a.second, b.second) < 0;
  }
};


const Symbol kNullSymbol;

typedef HASH_MAP<const char*, Symbol, HASH_FXN<const char*>, streq>
    SymbolsByNameMap;

typedef HASH_MAP<PointerStringPair, Symbol, PointerStringPairHash,
                 PointerStringPairEqual>
    SymbolsByParentMap;

typedef HASH_MAP<const char*, const FileDescriptor*, HASH_FXN<const char*>,
                 streq>
    FilesByNameMap;

typedef HASH_MAP<PointerStringPair, const FieldDescriptor*,
                 PointerStringPairHash, PointerStringPairEqual>
    FieldsByNameMap;

typedef HASH_MAP<DescriptorIntPair, const FieldDescriptor*,
                 PointerIntegerPairHash<DescriptorIntPair>,
                 std::equal_to<DescriptorIntPair>>
    FieldsByNumberMap;

typedef HASH_MAP<EnumIntPair, const EnumValueDescriptor*,
                 PointerIntegerPairHash<EnumIntPair>,
                 std::equal_to<EnumIntPair>>
    EnumValuesByNumberMap;
// This is a map rather than a hash-map, since we use it to iterate
// through all the extensions that extend a given Descriptor, and an
// ordered data structure that implements lower_bound is convenient
// for that.
typedef std::map<DescriptorIntPair, const FieldDescriptor*>
    ExtensionsGroupedByDescriptorMap;
typedef HASH_MAP<std::string, const SourceCodeInfo_Location*>
    LocationsByPathMap;

std::set<std::string>* NewAllowedProto3Extendee() {
  auto allowed_proto3_extendees = new std::set<std::string>;
  const char* kOptionNames[] = {
      "FileOptions",      "MessageOptions", "FieldOptions",  "EnumOptions",
      "EnumValueOptions", "ServiceOptions", "MethodOptions", "OneofOptions"};
  for (int i = 0; i < GOOGLE_ARRAYSIZE(kOptionNames); ++i) {
    // descriptor.proto has a different package name in opensource. We allow
    // both so the opensource protocol compiler can also compile internal
    // proto3 files with custom options. See: b/27567912
    allowed_proto3_extendees->insert(std::string("google.protobuf.") +
                                     kOptionNames[i]);
    // Split the word to trick the opensource processing scripts so they
    // will keep the origial package name.
    allowed_proto3_extendees->insert(std::string("proto") + "2." +
                                     kOptionNames[i]);
  }
  return allowed_proto3_extendees;
}

// Checks whether the extendee type is allowed in proto3.
// Only extensions to descriptor options are allowed. We use name comparison
// instead of comparing the descriptor directly because the extensions may be
// defined in a different pool.
bool AllowedExtendeeInProto3(const std::string& name) {
  static auto allowed_proto3_extendees =
      internal::OnShutdownDelete(NewAllowedProto3Extendee());
  return allowed_proto3_extendees->find(name) !=
         allowed_proto3_extendees->end();
}

}  // anonymous namespace

// ===================================================================
// DescriptorPool::Tables

class DescriptorPool::Tables {
 public:
  Tables();
  ~Tables();

  // Record the current state of the tables to the stack of checkpoints.
  // Each call to AddCheckpoint() must be paired with exactly one call to either
  // ClearLastCheckpoint() or RollbackToLastCheckpoint().
  //
  // This is used when building files, since some kinds of validation errors
  // cannot be detected until the file's descriptors have already been added to
  // the tables.
  //
  // This supports recursive checkpoints, since building a file may trigger
  // recursive building of other files. Note that recursive checkpoints are not
  // normally necessary; explicit dependencies are built prior to checkpointing.
  // So although we recursively build transitive imports, there is at most one
  // checkpoint in the stack during dependency building.
  //
  // Recursive checkpoints only arise during cross-linking of the descriptors.
  // Symbol references must be resolved, via DescriptorBuilder::FindSymbol and
  // friends. If the pending file references an unknown symbol
  // (e.g., it is not defined in the pending file's explicit dependencies), and
  // the pool is using a fallback database, and that database contains a file
  // defining that symbol, and that file has not yet been built by the pool,
  // the pool builds the file during cross-linking, leading to another
  // checkpoint.
  void AddCheckpoint();

  // Mark the last checkpoint as having cleared successfully, removing it from
  // the stack. If the stack is empty, all pending symbols will be committed.
  //
  // Note that this does not guarantee that the symbols added since the last
  // checkpoint won't be rolled back: if a checkpoint gets rolled back,
  // everything past that point gets rolled back, including symbols added after
  // checkpoints that were pushed onto the stack after it and marked as cleared.
  void ClearLastCheckpoint();

  // Roll back the Tables to the state of the checkpoint at the top of the
  // stack, removing everything that was added after that point.
  void RollbackToLastCheckpoint();

  // The stack of files which are currently being built.  Used to detect
  // cyclic dependencies when loading files from a DescriptorDatabase.  Not
  // used when fallback_database_ == nullptr.
  std::vector<std::string> pending_files_;

  // A set of files which we have tried to load from the fallback database
  // and encountered errors.  We will not attempt to load them again during
  // execution of the current public API call, but for compatibility with
  // legacy clients, this is cleared at the beginning of each public API call.
  // Not used when fallback_database_ == nullptr.
  HASH_SET<std::string> known_bad_files_;

  // A set of symbols which we have tried to load from the fallback database
  // and encountered errors. We will not attempt to load them again during
  // execution of the current public API call, but for compatibility with
  // legacy clients, this is cleared at the beginning of each public API call.
  HASH_SET<std::string> known_bad_symbols_;

  // The set of descriptors for which we've already loaded the full
  // set of extensions numbers from fallback_database_.
  HASH_SET<const Descriptor*> extensions_loaded_from_db_;

  // -----------------------------------------------------------------
  // Finding items.

  // Find symbols.  This returns a null Symbol (symbol.IsNull() is true)
  // if not found.
  inline Symbol FindSymbol(const std::string& key) const;

  // This implements the body of DescriptorPool::Find*ByName().  It should
  // really be a private method of DescriptorPool, but that would require
  // declaring Symbol in descriptor.h, which would drag all kinds of other
  // stuff into the header.  Yay C++.
  Symbol FindByNameHelper(const DescriptorPool* pool, const std::string& name);

  // These return nullptr if not found.
  inline const FileDescriptor* FindFile(const std::string& key) const;
  inline const FieldDescriptor* FindExtension(const Descriptor* extendee,
                                              int number) const;
  inline void FindAllExtensions(const Descriptor* extendee,
                                std::vector<const FieldDescriptor*>* out) const;

  // -----------------------------------------------------------------
  // Adding items.

  // These add items to the corresponding tables.  They return false if
  // the key already exists in the table.  For AddSymbol(), the string passed
  // in must be one that was constructed using AllocateString(), as it will
  // be used as a key in the symbols_by_name_ map without copying.
  bool AddSymbol(const std::string& full_name, Symbol symbol);
  bool AddFile(const FileDescriptor* file);
  bool AddExtension(const FieldDescriptor* field);

  // -----------------------------------------------------------------
  // Allocating memory.

  // Allocate an object which will be reclaimed when the pool is
  // destroyed.  Note that the object's destructor will never be called,
  // so its fields must be plain old data (primitive data types and
  // pointers).  All of the descriptor types are such objects.
  template <typename Type>
  Type* Allocate();

  // Allocate an array of objects which will be reclaimed when the
  // pool in destroyed.  Again, destructors are never called.
  template <typename Type>
  Type* AllocateArray(int count);

  // Allocate a string which will be destroyed when the pool is destroyed.
  // The string is initialized to the given value for convenience.
  std::string* AllocateString(const std::string& value);

  // Allocate empty string which will be destroyed when the pool is destroyed.
  std::string* AllocateEmptyString();

  // Allocate a internal::call_once which will be destroyed when the pool is
  // destroyed.
  internal::once_flag* AllocateOnceDynamic();

  // Allocate a protocol message object.  Some older versions of GCC have
  // trouble understanding explicit template instantiations in some cases, so
  // in those cases we have to pass a dummy pointer of the right type as the
  // parameter instead of specifying the type explicitly.
  template <typename Type>
  Type* AllocateMessage(Type* dummy = nullptr);

  // Allocate a FileDescriptorTables object.
  FileDescriptorTables* AllocateFileTables();

 private:
  std::vector<std::string*> strings_;  // All strings in the pool.
  std::vector<Message*> messages_;     // All messages in the pool.
  std::vector<internal::once_flag*>
      once_dynamics_;  // All internal::call_onces in the pool.
  std::vector<FileDescriptorTables*>
      file_tables_;                 // All file tables in the pool.
  std::vector<void*> allocations_;  // All other memory allocated in the pool.

  SymbolsByNameMap symbols_by_name_;
  FilesByNameMap files_by_name_;
  ExtensionsGroupedByDescriptorMap extensions_;

  struct CheckPoint {
    explicit CheckPoint(const Tables* tables)
        : strings_before_checkpoint(tables->strings_.size()),
          messages_before_checkpoint(tables->messages_.size()),
          once_dynamics_before_checkpoint(tables->once_dynamics_.size()),
          file_tables_before_checkpoint(tables->file_tables_.size()),
          allocations_before_checkpoint(tables->allocations_.size()),
          pending_symbols_before_checkpoint(
              tables->symbols_after_checkpoint_.size()),
          pending_files_before_checkpoint(
              tables->files_after_checkpoint_.size()),
          pending_extensions_before_checkpoint(
              tables->extensions_after_checkpoint_.size()) {}
    int strings_before_checkpoint;
    int messages_before_checkpoint;
    int once_dynamics_before_checkpoint;
    int file_tables_before_checkpoint;
    int allocations_before_checkpoint;
    int pending_symbols_before_checkpoint;
    int pending_files_before_checkpoint;
    int pending_extensions_before_checkpoint;
  };
  std::vector<CheckPoint> checkpoints_;
  std::vector<const char*> symbols_after_checkpoint_;
  std::vector<const char*> files_after_checkpoint_;
  std::vector<DescriptorIntPair> extensions_after_checkpoint_;

  // Allocate some bytes which will be reclaimed when the pool is
  // destroyed.
  void* AllocateBytes(int size);
};

// Contains tables specific to a particular file.  These tables are not
// modified once the file has been constructed, so they need not be
// protected by a mutex.  This makes operations that depend only on the
// contents of a single file -- e.g. Descriptor::FindFieldByName() --
// lock-free.
//
// For historical reasons, the definitions of the methods of
// FileDescriptorTables and DescriptorPool::Tables are interleaved below.
// These used to be a single class.
class FileDescriptorTables {
 public:
  FileDescriptorTables();
  ~FileDescriptorTables();

  // Empty table, used with placeholder files.
  inline static const FileDescriptorTables& GetEmptyInstance();

  // -----------------------------------------------------------------
  // Finding items.

  // Find symbols.  These return a null Symbol (symbol.IsNull() is true)
  // if not found.
  inline Symbol FindNestedSymbol(const void* parent,
                                 const std::string& name) const;
  inline Symbol FindNestedSymbolOfType(const void* parent,
                                       const std::string& name,
                                       const Symbol::Type type) const;

  // These return nullptr if not found.
  inline const FieldDescriptor* FindFieldByNumber(const Descriptor* parent,
                                                  int number) const;
  inline const FieldDescriptor* FindFieldByLowercaseName(
      const void* parent, const std::string& lowercase_name) const;
  inline const FieldDescriptor* FindFieldByCamelcaseName(
      const void* parent, const std::string& camelcase_name) const;
  inline const EnumValueDescriptor* FindEnumValueByNumber(
      const EnumDescriptor* parent, int number) const;
  // This creates a new EnumValueDescriptor if not found, in a thread-safe way.
  inline const EnumValueDescriptor* FindEnumValueByNumberCreatingIfUnknown(
      const EnumDescriptor* parent, int number) const;

  // -----------------------------------------------------------------
  // Adding items.

  // These add items to the corresponding tables.  They return false if
  // the key already exists in the table.  For AddAliasUnderParent(), the
  // string passed in must be one that was constructed using AllocateString(),
  // as it will be used as a key in the symbols_by_parent_ map without copying.
  bool AddAliasUnderParent(const void* parent, const std::string& name,
                           Symbol symbol);
  bool AddFieldByNumber(const FieldDescriptor* field);
  bool AddEnumValueByNumber(const EnumValueDescriptor* value);

  // Adds the field to the lowercase_name and camelcase_name maps.  Never
  // fails because we allow duplicates; the first field by the name wins.
  void AddFieldByStylizedNames(const FieldDescriptor* field);

  // Populates p->first->locations_by_path_ from p->second.
  // Unusual signature dictated by internal::call_once.
  static void BuildLocationsByPath(
      std::pair<const FileDescriptorTables*, const SourceCodeInfo*>* p);

  // Returns the location denoted by the specified path through info,
  // or nullptr if not found.
  // The value of info must be that of the corresponding FileDescriptor.
  // (Conceptually a pure function, but stateful as an optimisation.)
  const SourceCodeInfo_Location* GetSourceLocation(
      const std::vector<int>& path, const SourceCodeInfo* info) const;

  // Must be called after BuildFileImpl(), even if the build failed and
  // we are going to roll back to the last checkpoint.
  void FinalizeTables();

 private:
  const void* FindParentForFieldsByMap(const FieldDescriptor* field) const;
  static void FieldsByLowercaseNamesLazyInitStatic(
      const FileDescriptorTables* tables);
  void FieldsByLowercaseNamesLazyInitInternal() const;
  static void FieldsByCamelcaseNamesLazyInitStatic(
      const FileDescriptorTables* tables);
  void FieldsByCamelcaseNamesLazyInitInternal() const;

  SymbolsByParentMap symbols_by_parent_;
  mutable FieldsByNameMap fields_by_lowercase_name_;
  std::unique_ptr<FieldsByNameMap> fields_by_lowercase_name_tmp_;
  mutable internal::once_flag fields_by_lowercase_name_once_;
  mutable FieldsByNameMap fields_by_camelcase_name_;
  std::unique_ptr<FieldsByNameMap> fields_by_camelcase_name_tmp_;
  mutable internal::once_flag fields_by_camelcase_name_once_;
  FieldsByNumberMap fields_by_number_;  // Not including extensions.
  EnumValuesByNumberMap enum_values_by_number_;
  mutable EnumValuesByNumberMap unknown_enum_values_by_number_
      PROTOBUF_GUARDED_BY(unknown_enum_values_mu_);

  // Populated on first request to save space, hence constness games.
  mutable internal::once_flag locations_by_path_once_;
  mutable LocationsByPathMap locations_by_path_;

  // Mutex to protect the unknown-enum-value map due to dynamic
  // EnumValueDescriptor creation on unknown values.
  mutable internal::WrappedMutex unknown_enum_values_mu_;
};

DescriptorPool::Tables::Tables()
    // Start some hash-map and hash-set objects with a small # of buckets
    : known_bad_files_(3),
      known_bad_symbols_(3),
      extensions_loaded_from_db_(3),
      symbols_by_name_(3),
      files_by_name_(3) {}

DescriptorPool::Tables::~Tables() {
  GOOGLE_DCHECK(checkpoints_.empty());
  // Note that the deletion order is important, since the destructors of some
  // messages may refer to objects in allocations_.
  STLDeleteElements(&messages_);
  for (int i = 0; i < allocations_.size(); i++) {
    operator delete(allocations_[i]);
  }
  STLDeleteElements(&strings_);
  STLDeleteElements(&file_tables_);
  STLDeleteElements(&once_dynamics_);
}

FileDescriptorTables::FileDescriptorTables()
    // Initialize all the hash tables to start out with a small # of buckets.
    : symbols_by_parent_(3),
      fields_by_lowercase_name_(3),
      fields_by_lowercase_name_tmp_(new FieldsByNameMap()),
      fields_by_camelcase_name_(3),
      fields_by_camelcase_name_tmp_(new FieldsByNameMap()),
      fields_by_number_(3),
      enum_values_by_number_(3),
      unknown_enum_values_by_number_(3),
      locations_by_path_(3) {}

FileDescriptorTables::~FileDescriptorTables() {}

inline const FileDescriptorTables& FileDescriptorTables::GetEmptyInstance() {
  static auto file_descriptor_tables =
      internal::OnShutdownDelete(new FileDescriptorTables());
  return *file_descriptor_tables;
}

void DescriptorPool::Tables::AddCheckpoint() {
  checkpoints_.push_back(CheckPoint(this));
}

void DescriptorPool::Tables::ClearLastCheckpoint() {
  GOOGLE_DCHECK(!checkpoints_.empty());
  checkpoints_.pop_back();
  if (checkpoints_.empty()) {
    // All checkpoints have been cleared: we can now commit all of the pending
    // data.
    symbols_after_checkpoint_.clear();
    files_after_checkpoint_.clear();
    extensions_after_checkpoint_.clear();
  }
}

void DescriptorPool::Tables::RollbackToLastCheckpoint() {
  GOOGLE_DCHECK(!checkpoints_.empty());
  const CheckPoint& checkpoint = checkpoints_.back();

  for (int i = checkpoint.pending_symbols_before_checkpoint;
       i < symbols_after_checkpoint_.size(); i++) {
    symbols_by_name_.erase(symbols_after_checkpoint_[i]);
  }
  for (int i = checkpoint.pending_files_before_checkpoint;
       i < files_after_checkpoint_.size(); i++) {
    files_by_name_.erase(files_after_checkpoint_[i]);
  }
  for (int i = checkpoint.pending_extensions_before_checkpoint;
       i < extensions_after_checkpoint_.size(); i++) {
    extensions_.erase(extensions_after_checkpoint_[i]);
  }

  symbols_after_checkpoint_.resize(
      checkpoint.pending_symbols_before_checkpoint);
  files_after_checkpoint_.resize(checkpoint.pending_files_before_checkpoint);
  extensions_after_checkpoint_.resize(
      checkpoint.pending_extensions_before_checkpoint);

  STLDeleteContainerPointers(
      strings_.begin() + checkpoint.strings_before_checkpoint, strings_.end());
  STLDeleteContainerPointers(
      messages_.begin() + checkpoint.messages_before_checkpoint,
      messages_.end());
  STLDeleteContainerPointers(
      once_dynamics_.begin() + checkpoint.once_dynamics_before_checkpoint,
      once_dynamics_.end());
  STLDeleteContainerPointers(
      file_tables_.begin() + checkpoint.file_tables_before_checkpoint,
      file_tables_.end());
  for (int i = checkpoint.allocations_before_checkpoint;
       i < allocations_.size(); i++) {
    operator delete(allocations_[i]);
  }

  strings_.resize(checkpoint.strings_before_checkpoint);
  messages_.resize(checkpoint.messages_before_checkpoint);
  once_dynamics_.resize(checkpoint.once_dynamics_before_checkpoint);
  file_tables_.resize(checkpoint.file_tables_before_checkpoint);
  allocations_.resize(checkpoint.allocations_before_checkpoint);
  checkpoints_.pop_back();
}

// -------------------------------------------------------------------

inline Symbol DescriptorPool::Tables::FindSymbol(const std::string& key) const {
  const Symbol* result = FindOrNull(symbols_by_name_, key.c_str());
  if (result == nullptr) {
    return kNullSymbol;
  } else {
    return *result;
  }
}

inline Symbol FileDescriptorTables::FindNestedSymbol(
    const void* parent, const std::string& name) const {
  const Symbol* result = FindOrNull(
      symbols_by_parent_, PointerStringPair(parent, name.c_str()));
  if (result == nullptr) {
    return kNullSymbol;
  } else {
    return *result;
  }
}

inline Symbol FileDescriptorTables::FindNestedSymbolOfType(
    const void* parent, const std::string& name,
    const Symbol::Type type) const {
  Symbol result = FindNestedSymbol(parent, name);
  if (result.type != type) return kNullSymbol;
  return result;
}

Symbol DescriptorPool::Tables::FindByNameHelper(const DescriptorPool* pool,
                                                const std::string& name) {
  MutexLockMaybe lock(pool->mutex_);
  if (pool->fallback_database_ != nullptr) {
    known_bad_symbols_.clear();
    known_bad_files_.clear();
  }
  Symbol result = FindSymbol(name);

  if (result.IsNull() && pool->underlay_ != nullptr) {
    // Symbol not found; check the underlay.
    result = pool->underlay_->tables_->FindByNameHelper(pool->underlay_, name);
  }

  if (result.IsNull()) {
    // Symbol still not found, so check fallback database.
    if (pool->TryFindSymbolInFallbackDatabase(name)) {
      result = FindSymbol(name);
    }
  }

  return result;
}

inline const FileDescriptor* DescriptorPool::Tables::FindFile(
    const std::string& key) const {
  return FindPtrOrNull(files_by_name_, key.c_str());
}

inline const FieldDescriptor* FileDescriptorTables::FindFieldByNumber(
    const Descriptor* parent, int number) const {
  return FindPtrOrNull(fields_by_number_, std::make_pair(parent, number));
}

const void* FileDescriptorTables::FindParentForFieldsByMap(
    const FieldDescriptor* field) const {
  if (field->is_extension()) {
    if (field->extension_scope() == nullptr) {
      return field->file();
    } else {
      return field->extension_scope();
    }
  } else {
    return field->containing_type();
  }
}

void FileDescriptorTables::FieldsByLowercaseNamesLazyInitStatic(
    const FileDescriptorTables* tables) {
  tables->FieldsByLowercaseNamesLazyInitInternal();
}

void FileDescriptorTables::FieldsByLowercaseNamesLazyInitInternal() const {
  for (FieldsByNumberMap::const_iterator it = fields_by_number_.begin();
       it != fields_by_number_.end(); it++) {
    PointerStringPair lowercase_key(FindParentForFieldsByMap(it->second),
                                    it->second->lowercase_name().c_str());
    InsertIfNotPresent(&fields_by_lowercase_name_, lowercase_key,
                            it->second);
  }
}

inline const FieldDescriptor* FileDescriptorTables::FindFieldByLowercaseName(
    const void* parent, const std::string& lowercase_name) const {
  internal::call_once(
      fields_by_lowercase_name_once_,
      &FileDescriptorTables::FieldsByLowercaseNamesLazyInitStatic, this);
  return FindPtrOrNull(fields_by_lowercase_name_,
                            PointerStringPair(parent, lowercase_name.c_str()));
}

void FileDescriptorTables::FieldsByCamelcaseNamesLazyInitStatic(
    const FileDescriptorTables* tables) {
  tables->FieldsByCamelcaseNamesLazyInitInternal();
}

void FileDescriptorTables::FieldsByCamelcaseNamesLazyInitInternal() const {
  for (FieldsByNumberMap::const_iterator it = fields_by_number_.begin();
       it != fields_by_number_.end(); it++) {
    PointerStringPair camelcase_key(FindParentForFieldsByMap(it->second),
                                    it->second->camelcase_name().c_str());
    InsertIfNotPresent(&fields_by_camelcase_name_, camelcase_key,
                            it->second);
  }
}

inline const FieldDescriptor* FileDescriptorTables::FindFieldByCamelcaseName(
    const void* parent, const std::string& camelcase_name) const {
  internal::call_once(
      fields_by_camelcase_name_once_,
      FileDescriptorTables::FieldsByCamelcaseNamesLazyInitStatic, this);
  return FindPtrOrNull(fields_by_camelcase_name_,
                            PointerStringPair(parent, camelcase_name.c_str()));
}

inline const EnumValueDescriptor* FileDescriptorTables::FindEnumValueByNumber(
    const EnumDescriptor* parent, int number) const {
  return FindPtrOrNull(enum_values_by_number_,
                            std::make_pair(parent, number));
}

inline const EnumValueDescriptor*
FileDescriptorTables::FindEnumValueByNumberCreatingIfUnknown(
    const EnumDescriptor* parent, int number) const {
  // First try, with map of compiled-in values.
  {
    const EnumValueDescriptor* desc = FindPtrOrNull(
        enum_values_by_number_, std::make_pair(parent, number));
    if (desc != nullptr) {
      return desc;
    }
  }
  // Second try, with reader lock held on unknown enum values: common case.
  {
    ReaderMutexLock l(&unknown_enum_values_mu_);
    const EnumValueDescriptor* desc = FindPtrOrNull(
        unknown_enum_values_by_number_, std::make_pair(parent, number));
    if (desc != nullptr) {
      return desc;
    }
  }
  // If not found, try again with writer lock held, and create new descriptor if
  // necessary.
  {
    WriterMutexLock l(&unknown_enum_values_mu_);
    const EnumValueDescriptor* desc = FindPtrOrNull(
        unknown_enum_values_by_number_, std::make_pair(parent, number));
    if (desc != nullptr) {
      return desc;
    }

    // Create an EnumValueDescriptor dynamically. We don't insert it into the
    // EnumDescriptor (it's not a part of the enum as originally defined), but
    // we do insert it into the table so that we can return the same pointer
    // later.
    std::string enum_value_name = StringPrintf("UNKNOWN_ENUM_VALUE_%s_%d",
                                               parent->name().c_str(), number);
    DescriptorPool::Tables* tables = const_cast<DescriptorPool::Tables*>(
        DescriptorPool::generated_pool()->tables_.get());
    EnumValueDescriptor* result = tables->Allocate<EnumValueDescriptor>();
    result->name_ = tables->AllocateString(enum_value_name);
    result->full_name_ =
        tables->AllocateString(parent->full_name() + "." + enum_value_name);
    result->number_ = number;
    result->type_ = parent;
    result->options_ = &EnumValueOptions::default_instance();
    InsertIfNotPresent(&unknown_enum_values_by_number_,
                            std::make_pair(parent, number), result);
    return result;
  }
}

inline const FieldDescriptor* DescriptorPool::Tables::FindExtension(
    const Descriptor* extendee, int number) const {
  return FindPtrOrNull(extensions_, std::make_pair(extendee, number));
}

inline void DescriptorPool::Tables::FindAllExtensions(
    const Descriptor* extendee,
    std::vector<const FieldDescriptor*>* out) const {
  ExtensionsGroupedByDescriptorMap::const_iterator it =
      extensions_.lower_bound(std::make_pair(extendee, 0));
  for (; it != extensions_.end() && it->first.first == extendee; ++it) {
    out->push_back(it->second);
  }
}

// -------------------------------------------------------------------

bool DescriptorPool::Tables::AddSymbol(const std::string& full_name,
                                       Symbol symbol) {
  if (InsertIfNotPresent(&symbols_by_name_, full_name.c_str(), symbol)) {
    symbols_after_checkpoint_.push_back(full_name.c_str());
    return true;
  } else {
    return false;
  }
}

bool FileDescriptorTables::AddAliasUnderParent(const void* parent,
                                               const std::string& name,
                                               Symbol symbol) {
  PointerStringPair by_parent_key(parent, name.c_str());
  return InsertIfNotPresent(&symbols_by_parent_, by_parent_key, symbol);
}

bool DescriptorPool::Tables::AddFile(const FileDescriptor* file) {
  if (InsertIfNotPresent(&files_by_name_, file->name().c_str(), file)) {
    files_after_checkpoint_.push_back(file->name().c_str());
    return true;
  } else {
    return false;
  }
}

void FileDescriptorTables::FinalizeTables() {
  // Clean up the temporary maps used by AddFieldByStylizedNames().
  fields_by_lowercase_name_tmp_ = nullptr;
  fields_by_camelcase_name_tmp_ = nullptr;
}

void FileDescriptorTables::AddFieldByStylizedNames(
    const FieldDescriptor* field) {
  const void* parent = FindParentForFieldsByMap(field);

  // We want fields_by_{lower,camel}case_name_ to be lazily built, but
  // cross-link order determines which entry will be present in the case of a
  // conflict. So we use the temporary maps that get destroyed after
  // BuildFileImpl() to detect the conflicts, and only store the conflicts in
  // the map that will persist. We will then lazily populate the rest of the
  // entries from fields_by_number_.

  PointerStringPair lowercase_key(parent, field->lowercase_name().c_str());
  if (!InsertIfNotPresent(fields_by_lowercase_name_tmp_.get(),
                               lowercase_key, field)) {
    InsertIfNotPresent(
        &fields_by_lowercase_name_, lowercase_key,
        FindPtrOrNull(*fields_by_lowercase_name_tmp_, lowercase_key));
  }

  PointerStringPair camelcase_key(parent, field->camelcase_name().c_str());
  if (!InsertIfNotPresent(fields_by_camelcase_name_tmp_.get(),
                               camelcase_key, field)) {
    InsertIfNotPresent(
        &fields_by_camelcase_name_, camelcase_key,
        FindPtrOrNull(*fields_by_camelcase_name_tmp_, camelcase_key));
  }
}

bool FileDescriptorTables::AddFieldByNumber(const FieldDescriptor* field) {
  DescriptorIntPair key(field->containing_type(), field->number());
  return InsertIfNotPresent(&fields_by_number_, key, field);
}

bool FileDescriptorTables::AddEnumValueByNumber(
    const EnumValueDescriptor* value) {
  EnumIntPair key(value->type(), value->number());
  return InsertIfNotPresent(&enum_values_by_number_, key, value);
}

bool DescriptorPool::Tables::AddExtension(const FieldDescriptor* field) {
  DescriptorIntPair key(field->containing_type(), field->number());
  if (InsertIfNotPresent(&extensions_, key, field)) {
    extensions_after_checkpoint_.push_back(key);
    return true;
  } else {
    return false;
  }
}

// -------------------------------------------------------------------

template <typename Type>
Type* DescriptorPool::Tables::Allocate() {
  return reinterpret_cast<Type*>(AllocateBytes(sizeof(Type)));
}

template <typename Type>
Type* DescriptorPool::Tables::AllocateArray(int count) {
  return reinterpret_cast<Type*>(AllocateBytes(sizeof(Type) * count));
}

std::string* DescriptorPool::Tables::AllocateString(const std::string& value) {
  std::string* result = new std::string(value);
  strings_.push_back(result);
  return result;
}

std::string* DescriptorPool::Tables::AllocateEmptyString() {
  std::string* result = new std::string();
  strings_.push_back(result);
  return result;
}

internal::once_flag* DescriptorPool::Tables::AllocateOnceDynamic() {
  internal::once_flag* result = new internal::once_flag();
  once_dynamics_.push_back(result);
  return result;
}

template <typename Type>
Type* DescriptorPool::Tables::AllocateMessage(Type* /* dummy */) {
  Type* result = new Type;
  messages_.push_back(result);
  return result;
}

FileDescriptorTables* DescriptorPool::Tables::AllocateFileTables() {
  FileDescriptorTables* result = new FileDescriptorTables;
  file_tables_.push_back(result);
  return result;
}

void* DescriptorPool::Tables::AllocateBytes(int size) {
  // TODO(kenton):  Would it be worthwhile to implement this in some more
  // sophisticated way?  Probably not for the open source release, but for
  // internal use we could easily plug in one of our existing memory pool
  // allocators...
  if (size == 0) return nullptr;

  void* result = operator new(size);
  allocations_.push_back(result);
  return result;
}

void FileDescriptorTables::BuildLocationsByPath(
    std::pair<const FileDescriptorTables*, const SourceCodeInfo*>* p) {
  for (int i = 0, len = p->second->location_size(); i < len; ++i) {
    const SourceCodeInfo_Location* loc = &p->second->location().Get(i);
    p->first->locations_by_path_[Join(loc->path(), ",")] = loc;
  }
}

const SourceCodeInfo_Location* FileDescriptorTables::GetSourceLocation(
    const std::vector<int>& path, const SourceCodeInfo* info) const {
  std::pair<const FileDescriptorTables*, const SourceCodeInfo*> p(
      std::make_pair(this, info));
  internal::call_once(locations_by_path_once_,
                      FileDescriptorTables::BuildLocationsByPath, &p);
  return FindPtrOrNull(locations_by_path_, Join(path, ","));
}

// ===================================================================
// DescriptorPool

DescriptorPool::ErrorCollector::~ErrorCollector() {}

DescriptorPool::DescriptorPool()
    : mutex_(nullptr),
      fallback_database_(nullptr),
      default_error_collector_(nullptr),
      underlay_(nullptr),
      tables_(new Tables),
      enforce_dependencies_(true),
      lazily_build_dependencies_(false),
      allow_unknown_(false),
      enforce_weak_(false),
      disallow_enforce_utf8_(false) {}

DescriptorPool::DescriptorPool(DescriptorDatabase* fallback_database,
                               ErrorCollector* error_collector)
    : mutex_(new internal::WrappedMutex),
      fallback_database_(fallback_database),
      default_error_collector_(error_collector),
      underlay_(nullptr),
      tables_(new Tables),
      enforce_dependencies_(true),
      lazily_build_dependencies_(false),
      allow_unknown_(false),
      enforce_weak_(false),
      disallow_enforce_utf8_(false) {}

DescriptorPool::DescriptorPool(const DescriptorPool* underlay)
    : mutex_(nullptr),
      fallback_database_(nullptr),
      default_error_collector_(nullptr),
      underlay_(underlay),
      tables_(new Tables),
      enforce_dependencies_(true),
      lazily_build_dependencies_(false),
      allow_unknown_(false),
      enforce_weak_(false),
      disallow_enforce_utf8_(false) {}

DescriptorPool::~DescriptorPool() {
  if (mutex_ != nullptr) delete mutex_;
}

// DescriptorPool::BuildFile() defined later.
// DescriptorPool::BuildFileCollectingErrors() defined later.

void DescriptorPool::InternalDontEnforceDependencies() {
  enforce_dependencies_ = false;
}

void DescriptorPool::AddUnusedImportTrackFile(const std::string& file_name) {
  unused_import_track_files_.insert(file_name);
}

void DescriptorPool::ClearUnusedImportTrackFiles() {
  unused_import_track_files_.clear();
}

bool DescriptorPool::InternalIsFileLoaded(const std::string& filename) const {
  MutexLockMaybe lock(mutex_);
  return tables_->FindFile(filename) != nullptr;
}

// generated_pool ====================================================

namespace {


EncodedDescriptorDatabase* GeneratedDatabase() {
  static auto generated_database =
      internal::OnShutdownDelete(new EncodedDescriptorDatabase());
  return generated_database;
}

DescriptorPool* NewGeneratedPool() {
  auto generated_pool = new DescriptorPool(GeneratedDatabase());
  generated_pool->InternalSetLazilyBuildDependencies();
  return generated_pool;
}

}  // anonymous namespace

DescriptorPool* DescriptorPool::internal_generated_pool() {
  static DescriptorPool* generated_pool =
      internal::OnShutdownDelete(NewGeneratedPool());
  return generated_pool;
}

const DescriptorPool* DescriptorPool::generated_pool() {
  const DescriptorPool* pool = internal_generated_pool();
  // Ensure that descriptor.proto has been registered in the generated pool.
  DescriptorProto::descriptor();
  return pool;
}



void DescriptorPool::InternalAddGeneratedFile(
    const void* encoded_file_descriptor, int size) {
  // So, this function is called in the process of initializing the
  // descriptors for generated proto classes.  Each generated .pb.cc file
  // has an internal procedure called AddDescriptors() which is called at
  // process startup, and that function calls this one in order to register
  // the raw bytes of the FileDescriptorProto representing the file.
  //
  // We do not actually construct the descriptor objects right away.  We just
  // hang on to the bytes until they are actually needed.  We actually construct
  // the descriptor the first time one of the following things happens:
  // * Someone calls a method like descriptor(), GetDescriptor(), or
  //   GetReflection() on the generated types, which requires returning the
  //   descriptor or an object based on it.
  // * Someone looks up the descriptor in DescriptorPool::generated_pool().
  //
  // Once one of these happens, the DescriptorPool actually parses the
  // FileDescriptorProto and generates a FileDescriptor (and all its children)
  // based on it.
  //
  // Note that FileDescriptorProto is itself a generated protocol message.
  // Therefore, when we parse one, we have to be very careful to avoid using
  // any descriptor-based operations, since this might cause infinite recursion
  // or deadlock.
  GOOGLE_CHECK(GeneratedDatabase()->Add(encoded_file_descriptor, size));
}


// Find*By* methods ==================================================

// TODO(kenton):  There's a lot of repeated code here, but I'm not sure if
//   there's any good way to factor it out.  Think about this some time when
//   there's nothing more important to do (read: never).

const FileDescriptor* DescriptorPool::FindFileByName(
    const std::string& name) const {
  MutexLockMaybe lock(mutex_);
  if (fallback_database_ != nullptr) {
    tables_->known_bad_symbols_.clear();
    tables_->known_bad_files_.clear();
  }
  const FileDescriptor* result = tables_->FindFile(name);
  if (result != nullptr) return result;
  if (underlay_ != nullptr) {
    result = underlay_->FindFileByName(name);
    if (result != nullptr) return result;
  }
  if (TryFindFileInFallbackDatabase(name)) {
    result = tables_->FindFile(name);
    if (result != nullptr) return result;
  }
  return nullptr;
}

const FileDescriptor* DescriptorPool::FindFileContainingSymbol(
    const std::string& symbol_name) const {
  MutexLockMaybe lock(mutex_);
  if (fallback_database_ != nullptr) {
    tables_->known_bad_symbols_.clear();
    tables_->known_bad_files_.clear();
  }
  Symbol result = tables_->FindSymbol(symbol_name);
  if (!result.IsNull()) return result.GetFile();
  if (underlay_ != nullptr) {
    const FileDescriptor* file_result =
        underlay_->FindFileContainingSymbol(symbol_name);
    if (file_result != nullptr) return file_result;
  }
  if (TryFindSymbolInFallbackDatabase(symbol_name)) {
    result = tables_->FindSymbol(symbol_name);
    if (!result.IsNull()) return result.GetFile();
  }
  return nullptr;
}

const Descriptor* DescriptorPool::FindMessageTypeByName(
    const std::string& name) const {
  Symbol result = tables_->FindByNameHelper(this, name);
  return (result.type == Symbol::MESSAGE) ? result.descriptor : nullptr;
}

const FieldDescriptor* DescriptorPool::FindFieldByName(
    const std::string& name) const {
  Symbol result = tables_->FindByNameHelper(this, name);
  if (result.type == Symbol::FIELD &&
      !result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return nullptr;
  }
}

const FieldDescriptor* DescriptorPool::FindExtensionByName(
    const std::string& name) const {
  Symbol result = tables_->FindByNameHelper(this, name);
  if (result.type == Symbol::FIELD && result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return nullptr;
  }
}

const OneofDescriptor* DescriptorPool::FindOneofByName(
    const std::string& name) const {
  Symbol result = tables_->FindByNameHelper(this, name);
  return (result.type == Symbol::ONEOF) ? result.oneof_descriptor : nullptr;
}

const EnumDescriptor* DescriptorPool::FindEnumTypeByName(
    const std::string& name) const {
  Symbol result = tables_->FindByNameHelper(this, name);
  return (result.type == Symbol::ENUM) ? result.enum_descriptor : nullptr;
}

const EnumValueDescriptor* DescriptorPool::FindEnumValueByName(
    const std::string& name) const {
  Symbol result = tables_->FindByNameHelper(this, name);
  return (result.type == Symbol::ENUM_VALUE) ? result.enum_value_descriptor
                                             : nullptr;
}

const ServiceDescriptor* DescriptorPool::FindServiceByName(
    const std::string& name) const {
  Symbol result = tables_->FindByNameHelper(this, name);
  return (result.type == Symbol::SERVICE) ? result.service_descriptor : nullptr;
}

const MethodDescriptor* DescriptorPool::FindMethodByName(
    const std::string& name) const {
  Symbol result = tables_->FindByNameHelper(this, name);
  return (result.type == Symbol::METHOD) ? result.method_descriptor : nullptr;
}

const FieldDescriptor* DescriptorPool::FindExtensionByNumber(
    const Descriptor* extendee, int number) const {
  if (extendee->extension_range_count() == 0) return nullptr;
  // A faster path to reduce lock contention in finding extensions, assuming
  // most extensions will be cache hit.
  if (mutex_ != nullptr) {
    ReaderMutexLock lock(mutex_);
    const FieldDescriptor* result = tables_->FindExtension(extendee, number);
    if (result != nullptr) {
      return result;
    }
  }
  MutexLockMaybe lock(mutex_);
  if (fallback_database_ != nullptr) {
    tables_->known_bad_symbols_.clear();
    tables_->known_bad_files_.clear();
  }
  const FieldDescriptor* result = tables_->FindExtension(extendee, number);
  if (result != nullptr) {
    return result;
  }
  if (underlay_ != nullptr) {
    result = underlay_->FindExtensionByNumber(extendee, number);
    if (result != nullptr) return result;
  }
  if (TryFindExtensionInFallbackDatabase(extendee, number)) {
    result = tables_->FindExtension(extendee, number);
    if (result != nullptr) {
      return result;
    }
  }
  return nullptr;
}

const FieldDescriptor* DescriptorPool::FindExtensionByPrintableName(
    const Descriptor* extendee, const std::string& printable_name) const {
  if (extendee->extension_range_count() == 0) return nullptr;
  const FieldDescriptor* result = FindExtensionByName(printable_name);
  if (result != nullptr && result->containing_type() == extendee) {
    return result;
  }
  if (extendee->options().message_set_wire_format()) {
    // MessageSet extensions may be identified by type name.
    const Descriptor* type = FindMessageTypeByName(printable_name);
    if (type != nullptr) {
      // Look for a matching extension in the foreign type's scope.
      const int type_extension_count = type->extension_count();
      for (int i = 0; i < type_extension_count; i++) {
        const FieldDescriptor* extension = type->extension(i);
        if (extension->containing_type() == extendee &&
            extension->type() == FieldDescriptor::TYPE_MESSAGE &&
            extension->is_optional() && extension->message_type() == type) {
          // Found it.
          return extension;
        }
      }
    }
  }
  return nullptr;
}

void DescriptorPool::FindAllExtensions(
    const Descriptor* extendee,
    std::vector<const FieldDescriptor*>* out) const {
  MutexLockMaybe lock(mutex_);
  if (fallback_database_ != nullptr) {
    tables_->known_bad_symbols_.clear();
    tables_->known_bad_files_.clear();
  }

  // Initialize tables_->extensions_ from the fallback database first
  // (but do this only once per descriptor).
  if (fallback_database_ != nullptr &&
      tables_->extensions_loaded_from_db_.count(extendee) == 0) {
    std::vector<int> numbers;
    if (fallback_database_->FindAllExtensionNumbers(extendee->full_name(),
                                                    &numbers)) {
      for (int i = 0; i < numbers.size(); ++i) {
        int number = numbers[i];
        if (tables_->FindExtension(extendee, number) == nullptr) {
          TryFindExtensionInFallbackDatabase(extendee, number);
        }
      }
      tables_->extensions_loaded_from_db_.insert(extendee);
    }
  }

  tables_->FindAllExtensions(extendee, out);
  if (underlay_ != nullptr) {
    underlay_->FindAllExtensions(extendee, out);
  }
}


// -------------------------------------------------------------------

const FieldDescriptor* Descriptor::FindFieldByNumber(int key) const {
  const FieldDescriptor* result = file()->tables_->FindFieldByNumber(this, key);
  if (result == nullptr || result->is_extension()) {
    return nullptr;
  } else {
    return result;
  }
}

const FieldDescriptor* Descriptor::FindFieldByLowercaseName(
    const std::string& key) const {
  const FieldDescriptor* result =
      file()->tables_->FindFieldByLowercaseName(this, key);
  if (result == nullptr || result->is_extension()) {
    return nullptr;
  } else {
    return result;
  }
}

const FieldDescriptor* Descriptor::FindFieldByCamelcaseName(
    const std::string& key) const {
  const FieldDescriptor* result =
      file()->tables_->FindFieldByCamelcaseName(this, key);
  if (result == nullptr || result->is_extension()) {
    return nullptr;
  } else {
    return result;
  }
}

const FieldDescriptor* Descriptor::FindFieldByName(
    const std::string& key) const {
  Symbol result =
      file()->tables_->FindNestedSymbolOfType(this, key, Symbol::FIELD);
  if (!result.IsNull() && !result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return nullptr;
  }
}

const OneofDescriptor* Descriptor::FindOneofByName(
    const std::string& key) const {
  Symbol result =
      file()->tables_->FindNestedSymbolOfType(this, key, Symbol::ONEOF);
  if (!result.IsNull()) {
    return result.oneof_descriptor;
  } else {
    return nullptr;
  }
}

const FieldDescriptor* Descriptor::FindExtensionByName(
    const std::string& key) const {
  Symbol result =
      file()->tables_->FindNestedSymbolOfType(this, key, Symbol::FIELD);
  if (!result.IsNull() && result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return nullptr;
  }
}

const FieldDescriptor* Descriptor::FindExtensionByLowercaseName(
    const std::string& key) const {
  const FieldDescriptor* result =
      file()->tables_->FindFieldByLowercaseName(this, key);
  if (result == nullptr || !result->is_extension()) {
    return nullptr;
  } else {
    return result;
  }
}

const FieldDescriptor* Descriptor::FindExtensionByCamelcaseName(
    const std::string& key) const {
  const FieldDescriptor* result =
      file()->tables_->FindFieldByCamelcaseName(this, key);
  if (result == nullptr || !result->is_extension()) {
    return nullptr;
  } else {
    return result;
  }
}

const Descriptor* Descriptor::FindNestedTypeByName(
    const std::string& key) const {
  Symbol result =
      file()->tables_->FindNestedSymbolOfType(this, key, Symbol::MESSAGE);
  if (!result.IsNull()) {
    return result.descriptor;
  } else {
    return nullptr;
  }
}

const EnumDescriptor* Descriptor::FindEnumTypeByName(
    const std::string& key) const {
  Symbol result =
      file()->tables_->FindNestedSymbolOfType(this, key, Symbol::ENUM);
  if (!result.IsNull()) {
    return result.enum_descriptor;
  } else {
    return nullptr;
  }
}

const EnumValueDescriptor* Descriptor::FindEnumValueByName(
    const std::string& key) const {
  Symbol result =
      file()->tables_->FindNestedSymbolOfType(this, key, Symbol::ENUM_VALUE);
  if (!result.IsNull()) {
    return result.enum_value_descriptor;
  } else {
    return nullptr;
  }
}

const EnumValueDescriptor* EnumDescriptor::FindValueByName(
    const std::string& key) const {
  Symbol result =
      file()->tables_->FindNestedSymbolOfType(this, key, Symbol::ENUM_VALUE);
  if (!result.IsNull()) {
    return result.enum_value_descriptor;
  } else {
    return nullptr;
  }
}

const EnumValueDescriptor* EnumDescriptor::FindValueByNumber(int key) const {
  return file()->tables_->FindEnumValueByNumber(this, key);
}

const EnumValueDescriptor* EnumDescriptor::FindValueByNumberCreatingIfUnknown(
    int key) const {
  return file()->tables_->FindEnumValueByNumberCreatingIfUnknown(this, key);
}

const MethodDescriptor* ServiceDescriptor::FindMethodByName(
    const std::string& key) const {
  Symbol result =
      file()->tables_->FindNestedSymbolOfType(this, key, Symbol::METHOD);
  if (!result.IsNull()) {
    return result.method_descriptor;
  } else {
    return nullptr;
  }
}

const Descriptor* FileDescriptor::FindMessageTypeByName(
    const std::string& key) const {
  Symbol result = tables_->FindNestedSymbolOfType(this, key, Symbol::MESSAGE);
  if (!result.IsNull()) {
    return result.descriptor;
  } else {
    return nullptr;
  }
}

const EnumDescriptor* FileDescriptor::FindEnumTypeByName(
    const std::string& key) const {
  Symbol result = tables_->FindNestedSymbolOfType(this, key, Symbol::ENUM);
  if (!result.IsNull()) {
    return result.enum_descriptor;
  } else {
    return nullptr;
  }
}

const EnumValueDescriptor* FileDescriptor::FindEnumValueByName(
    const std::string& key) const {
  Symbol result =
      tables_->FindNestedSymbolOfType(this, key, Symbol::ENUM_VALUE);
  if (!result.IsNull()) {
    return result.enum_value_descriptor;
  } else {
    return nullptr;
  }
}

const ServiceDescriptor* FileDescriptor::FindServiceByName(
    const std::string& key) const {
  Symbol result = tables_->FindNestedSymbolOfType(this, key, Symbol::SERVICE);
  if (!result.IsNull()) {
    return result.service_descriptor;
  } else {
    return nullptr;
  }
}

const FieldDescriptor* FileDescriptor::FindExtensionByName(
    const std::string& key) const {
  Symbol result = tables_->FindNestedSymbolOfType(this, key, Symbol::FIELD);
  if (!result.IsNull() && result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return nullptr;
  }
}

const FieldDescriptor* FileDescriptor::FindExtensionByLowercaseName(
    const std::string& key) const {
  const FieldDescriptor* result = tables_->FindFieldByLowercaseName(this, key);
  if (result == nullptr || !result->is_extension()) {
    return nullptr;
  } else {
    return result;
  }
}

const FieldDescriptor* FileDescriptor::FindExtensionByCamelcaseName(
    const std::string& key) const {
  const FieldDescriptor* result = tables_->FindFieldByCamelcaseName(this, key);
  if (result == nullptr || !result->is_extension()) {
    return nullptr;
  } else {
    return result;
  }
}

void Descriptor::ExtensionRange::CopyTo(
    DescriptorProto_ExtensionRange* proto) const {
  proto->set_start(this->start);
  proto->set_end(this->end);
  if (options_ != &ExtensionRangeOptions::default_instance()) {
    *proto->mutable_options() = *options_;
  }
}

const Descriptor::ExtensionRange*
Descriptor::FindExtensionRangeContainingNumber(int number) const {
  // Linear search should be fine because we don't expect a message to have
  // more than a couple extension ranges.
  for (int i = 0; i < extension_range_count(); i++) {
    if (number >= extension_range(i)->start &&
        number < extension_range(i)->end) {
      return extension_range(i);
    }
  }
  return nullptr;
}

const Descriptor::ReservedRange* Descriptor::FindReservedRangeContainingNumber(
    int number) const {
  // TODO(chrisn): Consider a non-linear search.
  for (int i = 0; i < reserved_range_count(); i++) {
    if (number >= reserved_range(i)->start && number < reserved_range(i)->end) {
      return reserved_range(i);
    }
  }
  return nullptr;
}

const EnumDescriptor::ReservedRange*
EnumDescriptor::FindReservedRangeContainingNumber(int number) const {
  // TODO(chrisn): Consider a non-linear search.
  for (int i = 0; i < reserved_range_count(); i++) {
    if (number >= reserved_range(i)->start &&
        number <= reserved_range(i)->end) {
      return reserved_range(i);
    }
  }
  return nullptr;
}

// -------------------------------------------------------------------

bool DescriptorPool::TryFindFileInFallbackDatabase(
    const std::string& name) const {
  if (fallback_database_ == nullptr) return false;

  if (tables_->known_bad_files_.count(name) > 0) return false;

  FileDescriptorProto file_proto;
  if (!fallback_database_->FindFileByName(name, &file_proto) ||
      BuildFileFromDatabase(file_proto) == nullptr) {
    tables_->known_bad_files_.insert(name);
    return false;
  }
  return true;
}

bool DescriptorPool::IsSubSymbolOfBuiltType(const std::string& name) const {
  std::string prefix = name;
  for (;;) {
    std::string::size_type dot_pos = prefix.find_last_of('.');
    if (dot_pos == std::string::npos) {
      break;
    }
    prefix = prefix.substr(0, dot_pos);
    Symbol symbol = tables_->FindSymbol(prefix);
    // If the symbol type is anything other than PACKAGE, then its complete
    // definition is already known.
    if (!symbol.IsNull() && symbol.type != Symbol::PACKAGE) {
      return true;
    }
  }
  if (underlay_ != nullptr) {
    // Check to see if any prefix of this symbol exists in the underlay.
    return underlay_->IsSubSymbolOfBuiltType(name);
  }
  return false;
}

bool DescriptorPool::TryFindSymbolInFallbackDatabase(
    const std::string& name) const {
  if (fallback_database_ == nullptr) return false;

  if (tables_->known_bad_symbols_.count(name) > 0) return false;

  FileDescriptorProto file_proto;
  if (  // We skip looking in the fallback database if the name is a sub-symbol
        // of any descriptor that already exists in the descriptor pool (except
        // for package descriptors).  This is valid because all symbols except
        // for packages are defined in a single file, so if the symbol exists
        // then we should already have its definition.
        //
        // The other reason to do this is to support "overriding" type
        // definitions by merging two databases that define the same type. (Yes,
        // people do this.)  The main difficulty with making this work is that
        // FindFileContainingSymbol() is allowed to return both false positives
        // (e.g., SimpleDescriptorDatabase, UpgradedDescriptorDatabase) and
        // false negatives (e.g. ProtoFileParser, SourceTreeDescriptorDatabase).
        // When two such databases are merged, looking up a non-existent
        // sub-symbol of a type that already exists in the descriptor pool can
        // result in an attempt to load multiple definitions of the same type.
        // The check below avoids this.
      IsSubSymbolOfBuiltType(name)

      // Look up file containing this symbol in fallback database.
      || !fallback_database_->FindFileContainingSymbol(name, &file_proto)

      // Check if we've already built this file. If so, it apparently doesn't
      // contain the symbol we're looking for.  Some DescriptorDatabases
      // return false positives.
      || tables_->FindFile(file_proto.name()) != nullptr

      // Build the file.
      || BuildFileFromDatabase(file_proto) == nullptr) {
    tables_->known_bad_symbols_.insert(name);
    return false;
  }

  return true;
}

bool DescriptorPool::TryFindExtensionInFallbackDatabase(
    const Descriptor* containing_type, int field_number) const {
  if (fallback_database_ == nullptr) return false;

  FileDescriptorProto file_proto;
  if (!fallback_database_->FindFileContainingExtension(
          containing_type->full_name(), field_number, &file_proto)) {
    return false;
  }

  if (tables_->FindFile(file_proto.name()) != nullptr) {
    // We've already loaded this file, and it apparently doesn't contain the
    // extension we're looking for.  Some DescriptorDatabases return false
    // positives.
    return false;
  }

  if (BuildFileFromDatabase(file_proto) == nullptr) {
    return false;
  }

  return true;
}

// ===================================================================

bool FieldDescriptor::is_map_message_type() const {
  return message_type_->options().map_entry();
}

std::string FieldDescriptor::DefaultValueAsString(
    bool quote_string_type) const {
  GOOGLE_CHECK(has_default_value()) << "No default value";
  switch (cpp_type()) {
    case CPPTYPE_INT32:
      return StrCat(default_value_int32());
      break;
    case CPPTYPE_INT64:
      return StrCat(default_value_int64());
      break;
    case CPPTYPE_UINT32:
      return StrCat(default_value_uint32());
      break;
    case CPPTYPE_UINT64:
      return StrCat(default_value_uint64());
      break;
    case CPPTYPE_FLOAT:
      return SimpleFtoa(default_value_float());
      break;
    case CPPTYPE_DOUBLE:
      return SimpleDtoa(default_value_double());
      break;
    case CPPTYPE_BOOL:
      return default_value_bool() ? "true" : "false";
      break;
    case CPPTYPE_STRING:
      if (quote_string_type) {
        return "\"" + CEscape(default_value_string()) + "\"";
      } else {
        if (type() == TYPE_BYTES) {
          return CEscape(default_value_string());
        } else {
          return default_value_string();
        }
      }
      break;
    case CPPTYPE_ENUM:
      return default_value_enum()->name();
      break;
    case CPPTYPE_MESSAGE:
      GOOGLE_LOG(DFATAL) << "Messages can't have default values!";
      break;
  }
  GOOGLE_LOG(FATAL) << "Can't get here: failed to get default value as string";
  return "";
}

// CopyTo methods ====================================================

void FileDescriptor::CopyTo(FileDescriptorProto* proto) const {
  proto->set_name(name());
  if (!package().empty()) proto->set_package(package());
  // TODO(liujisi): Also populate when syntax="proto2".
  if (syntax() == SYNTAX_PROTO3) proto->set_syntax(SyntaxName(syntax()));

  for (int i = 0; i < dependency_count(); i++) {
    proto->add_dependency(dependency(i)->name());
  }

  for (int i = 0; i < public_dependency_count(); i++) {
    proto->add_public_dependency(public_dependencies_[i]);
  }

  for (int i = 0; i < weak_dependency_count(); i++) {
    proto->add_weak_dependency(weak_dependencies_[i]);
  }

  for (int i = 0; i < message_type_count(); i++) {
    message_type(i)->CopyTo(proto->add_message_type());
  }
  for (int i = 0; i < enum_type_count(); i++) {
    enum_type(i)->CopyTo(proto->add_enum_type());
  }
  for (int i = 0; i < service_count(); i++) {
    service(i)->CopyTo(proto->add_service());
  }
  for (int i = 0; i < extension_count(); i++) {
    extension(i)->CopyTo(proto->add_extension());
  }

  if (&options() != &FileOptions::default_instance()) {
    proto->mutable_options()->CopyFrom(options());
  }
}

void FileDescriptor::CopyJsonNameTo(FileDescriptorProto* proto) const {
  if (message_type_count() != proto->message_type_size() ||
      extension_count() != proto->extension_size()) {
    GOOGLE_LOG(ERROR) << "Cannot copy json_name to a proto of a different size.";
    return;
  }
  for (int i = 0; i < message_type_count(); i++) {
    message_type(i)->CopyJsonNameTo(proto->mutable_message_type(i));
  }
  for (int i = 0; i < extension_count(); i++) {
    extension(i)->CopyJsonNameTo(proto->mutable_extension(i));
  }
}

void FileDescriptor::CopySourceCodeInfoTo(FileDescriptorProto* proto) const {
  if (source_code_info_ &&
      source_code_info_ != &SourceCodeInfo::default_instance()) {
    proto->mutable_source_code_info()->CopyFrom(*source_code_info_);
  }
}

void Descriptor::CopyTo(DescriptorProto* proto) const {
  proto->set_name(name());

  for (int i = 0; i < field_count(); i++) {
    field(i)->CopyTo(proto->add_field());
  }
  for (int i = 0; i < oneof_decl_count(); i++) {
    oneof_decl(i)->CopyTo(proto->add_oneof_decl());
  }
  for (int i = 0; i < nested_type_count(); i++) {
    nested_type(i)->CopyTo(proto->add_nested_type());
  }
  for (int i = 0; i < enum_type_count(); i++) {
    enum_type(i)->CopyTo(proto->add_enum_type());
  }
  for (int i = 0; i < extension_range_count(); i++) {
    extension_range(i)->CopyTo(proto->add_extension_range());
  }
  for (int i = 0; i < extension_count(); i++) {
    extension(i)->CopyTo(proto->add_extension());
  }
  for (int i = 0; i < reserved_range_count(); i++) {
    DescriptorProto::ReservedRange* range = proto->add_reserved_range();
    range->set_start(reserved_range(i)->start);
    range->set_end(reserved_range(i)->end);
  }
  for (int i = 0; i < reserved_name_count(); i++) {
    proto->add_reserved_name(reserved_name(i));
  }

  if (&options() != &MessageOptions::default_instance()) {
    proto->mutable_options()->CopyFrom(options());
  }
}

void Descriptor::CopyJsonNameTo(DescriptorProto* proto) const {
  if (field_count() != proto->field_size() ||
      nested_type_count() != proto->nested_type_size() ||
      extension_count() != proto->extension_size()) {
    GOOGLE_LOG(ERROR) << "Cannot copy json_name to a proto of a different size.";
    return;
  }
  for (int i = 0; i < field_count(); i++) {
    field(i)->CopyJsonNameTo(proto->mutable_field(i));
  }
  for (int i = 0; i < nested_type_count(); i++) {
    nested_type(i)->CopyJsonNameTo(proto->mutable_nested_type(i));
  }
  for (int i = 0; i < extension_count(); i++) {
    extension(i)->CopyJsonNameTo(proto->mutable_extension(i));
  }
}

void FieldDescriptor::CopyTo(FieldDescriptorProto* proto) const {
  proto->set_name(name());
  proto->set_number(number());
  if (has_json_name_) {
    proto->set_json_name(json_name());
  }

  // Some compilers do not allow static_cast directly between two enum types,
  // so we must cast to int first.
  proto->set_label(static_cast<FieldDescriptorProto::Label>(
      implicit_cast<int>(label())));
  proto->set_type(static_cast<FieldDescriptorProto::Type>(
      implicit_cast<int>(type())));

  if (is_extension()) {
    if (!containing_type()->is_unqualified_placeholder_) {
      proto->set_extendee(".");
    }
    proto->mutable_extendee()->append(containing_type()->full_name());
  }

  if (cpp_type() == CPPTYPE_MESSAGE) {
    if (message_type()->is_placeholder_) {
      // We don't actually know if the type is a message type.  It could be
      // an enum.
      proto->clear_type();
    }

    if (!message_type()->is_unqualified_placeholder_) {
      proto->set_type_name(".");
    }
    proto->mutable_type_name()->append(message_type()->full_name());
  } else if (cpp_type() == CPPTYPE_ENUM) {
    if (!enum_type()->is_unqualified_placeholder_) {
      proto->set_type_name(".");
    }
    proto->mutable_type_name()->append(enum_type()->full_name());
  }

  if (has_default_value()) {
    proto->set_default_value(DefaultValueAsString(false));
  }

  if (containing_oneof() != nullptr && !is_extension()) {
    proto->set_oneof_index(containing_oneof()->index());
  }

  if (&options() != &FieldOptions::default_instance()) {
    proto->mutable_options()->CopyFrom(options());
  }
}

void FieldDescriptor::CopyJsonNameTo(FieldDescriptorProto* proto) const {
  proto->set_json_name(json_name());
}

void OneofDescriptor::CopyTo(OneofDescriptorProto* proto) const {
  proto->set_name(name());
  if (&options() != &OneofOptions::default_instance()) {
    proto->mutable_options()->CopyFrom(options());
  }
}

void EnumDescriptor::CopyTo(EnumDescriptorProto* proto) const {
  proto->set_name(name());

  for (int i = 0; i < value_count(); i++) {
    value(i)->CopyTo(proto->add_value());
  }
  for (int i = 0; i < reserved_range_count(); i++) {
    EnumDescriptorProto::EnumReservedRange* range = proto->add_reserved_range();
    range->set_start(reserved_range(i)->start);
    range->set_end(reserved_range(i)->end);
  }
  for (int i = 0; i < reserved_name_count(); i++) {
    proto->add_reserved_name(reserved_name(i));
  }

  if (&options() != &EnumOptions::default_instance()) {
    proto->mutable_options()->CopyFrom(options());
  }
}

void EnumValueDescriptor::CopyTo(EnumValueDescriptorProto* proto) const {
  proto->set_name(name());
  proto->set_number(number());

  if (&options() != &EnumValueOptions::default_instance()) {
    proto->mutable_options()->CopyFrom(options());
  }
}

void ServiceDescriptor::CopyTo(ServiceDescriptorProto* proto) const {
  proto->set_name(name());

  for (int i = 0; i < method_count(); i++) {
    method(i)->CopyTo(proto->add_method());
  }

  if (&options() != &ServiceOptions::default_instance()) {
    proto->mutable_options()->CopyFrom(options());
  }
}

void MethodDescriptor::CopyTo(MethodDescriptorProto* proto) const {
  proto->set_name(name());

  if (!input_type()->is_unqualified_placeholder_) {
    proto->set_input_type(".");
  }
  proto->mutable_input_type()->append(input_type()->full_name());

  if (!output_type()->is_unqualified_placeholder_) {
    proto->set_output_type(".");
  }
  proto->mutable_output_type()->append(output_type()->full_name());

  if (&options() != &MethodOptions::default_instance()) {
    proto->mutable_options()->CopyFrom(options());
  }

  if (client_streaming_) {
    proto->set_client_streaming(true);
  }
  if (server_streaming_) {
    proto->set_server_streaming(true);
  }
}

// DebugString methods ===============================================

namespace {

bool RetrieveOptionsAssumingRightPool(
    int depth, const Message& options,
    std::vector<std::string>* option_entries) {
  option_entries->clear();
  const Reflection* reflection = options.GetReflection();
  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(options, &fields);
  for (int i = 0; i < fields.size(); i++) {
    int count = 1;
    bool repeated = false;
    if (fields[i]->is_repeated()) {
      count = reflection->FieldSize(options, fields[i]);
      repeated = true;
    }
    for (int j = 0; j < count; j++) {
      std::string fieldval;
      if (fields[i]->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
        std::string tmp;
        TextFormat::Printer printer;
        printer.SetInitialIndentLevel(depth + 1);
        printer.PrintFieldValueToString(options, fields[i], repeated ? j : -1,
                                        &tmp);
        fieldval.append("{\n");
        fieldval.append(tmp);
        fieldval.append(depth * 2, ' ');
        fieldval.append("}");
      } else {
        TextFormat::PrintFieldValueToString(options, fields[i],
                                            repeated ? j : -1, &fieldval);
      }
      std::string name;
      if (fields[i]->is_extension()) {
        name = "(." + fields[i]->full_name() + ")";
      } else {
        name = fields[i]->name();
      }
      option_entries->push_back(name + " = " + fieldval);
    }
  }
  return !option_entries->empty();
}

// Used by each of the option formatters.
bool RetrieveOptions(int depth, const Message& options,
                     const DescriptorPool* pool,
                     std::vector<std::string>* option_entries) {
  // When printing custom options for a descriptor, we must use an options
  // message built on top of the same DescriptorPool where the descriptor
  // is coming from. This is to ensure we are interpreting custom options
  // against the right pool.
  if (options.GetDescriptor()->file()->pool() == pool) {
    return RetrieveOptionsAssumingRightPool(depth, options, option_entries);
  } else {
    const Descriptor* option_descriptor =
        pool->FindMessageTypeByName(options.GetDescriptor()->full_name());
    if (option_descriptor == nullptr) {
      // descriptor.proto is not in the pool. This means no custom options are
      // used so we are safe to proceed with the compiled options message type.
      return RetrieveOptionsAssumingRightPool(depth, options, option_entries);
    }
    DynamicMessageFactory factory;
    std::unique_ptr<Message> dynamic_options(
        factory.GetPrototype(option_descriptor)->New());
    if (dynamic_options->ParseFromString(options.SerializeAsString())) {
      return RetrieveOptionsAssumingRightPool(depth, *dynamic_options,
                                              option_entries);
    } else {
      GOOGLE_LOG(ERROR) << "Found invalid proto option data for: "
                 << options.GetDescriptor()->full_name();
      return RetrieveOptionsAssumingRightPool(depth, options, option_entries);
    }
  }
}

// Formats options that all appear together in brackets. Does not include
// brackets.
bool FormatBracketedOptions(int depth, const Message& options,
                            const DescriptorPool* pool, std::string* output) {
  std::vector<std::string> all_options;
  if (RetrieveOptions(depth, options, pool, &all_options)) {
    output->append(Join(all_options, ", "));
  }
  return !all_options.empty();
}

// Formats options one per line
bool FormatLineOptions(int depth, const Message& options,
                       const DescriptorPool* pool, std::string* output) {
  std::string prefix(depth * 2, ' ');
  std::vector<std::string> all_options;
  if (RetrieveOptions(depth, options, pool, &all_options)) {
    for (int i = 0; i < all_options.size(); i++) {
      strings::SubstituteAndAppend(output, "$0option $1;\n", prefix,
                                   all_options[i]);
    }
  }
  return !all_options.empty();
}

class SourceLocationCommentPrinter {
 public:
  template <typename DescType>
  SourceLocationCommentPrinter(const DescType* desc, const std::string& prefix,
                               const DebugStringOptions& options)
      : options_(options), prefix_(prefix) {
    // Perform the SourceLocation lookup only if we're including user comments,
    // because the lookup is fairly expensive.
    have_source_loc_ =
        options.include_comments && desc->GetSourceLocation(&source_loc_);
  }
  SourceLocationCommentPrinter(const FileDescriptor* file,
                               const std::vector<int>& path,
                               const std::string& prefix,
                               const DebugStringOptions& options)
      : options_(options), prefix_(prefix) {
    // Perform the SourceLocation lookup only if we're including user comments,
    // because the lookup is fairly expensive.
    have_source_loc_ =
        options.include_comments && file->GetSourceLocation(path, &source_loc_);
  }
  void AddPreComment(std::string* output) {
    if (have_source_loc_) {
      // Detached leading comments.
      for (int i = 0; i < source_loc_.leading_detached_comments.size(); ++i) {
        *output += FormatComment(source_loc_.leading_detached_comments[i]);
        *output += "\n";
      }
      // Attached leading comments.
      if (!source_loc_.leading_comments.empty()) {
        *output += FormatComment(source_loc_.leading_comments);
      }
    }
  }
  void AddPostComment(std::string* output) {
    if (have_source_loc_ && source_loc_.trailing_comments.size() > 0) {
      *output += FormatComment(source_loc_.trailing_comments);
    }
  }

  // Format comment such that each line becomes a full-line C++-style comment in
  // the DebugString() output.
  std::string FormatComment(const std::string& comment_text) {
    std::string stripped_comment = comment_text;
    StripWhitespace(&stripped_comment);
    std::vector<std::string> lines = Split(stripped_comment, "\n");
    std::string output;
    for (int i = 0; i < lines.size(); ++i) {
      const std::string& line = lines[i];
      strings::SubstituteAndAppend(&output, "$0// $1\n", prefix_, line);
    }
    return output;
  }

 private:

  bool have_source_loc_;
  SourceLocation source_loc_;
  DebugStringOptions options_;
  std::string prefix_;
};

}  // anonymous namespace

std::string FileDescriptor::DebugString() const {
  DebugStringOptions options;  // default options
  return DebugStringWithOptions(options);
}

std::string FileDescriptor::DebugStringWithOptions(
    const DebugStringOptions& debug_string_options) const {
  std::string contents;
  {
    std::vector<int> path;
    path.push_back(FileDescriptorProto::kSyntaxFieldNumber);
    SourceLocationCommentPrinter syntax_comment(this, path, "",
                                                debug_string_options);
    syntax_comment.AddPreComment(&contents);
    strings::SubstituteAndAppend(&contents, "syntax = \"$0\";\n\n",
                                 SyntaxName(syntax()));
    syntax_comment.AddPostComment(&contents);
  }

  SourceLocationCommentPrinter comment_printer(this, "", debug_string_options);
  comment_printer.AddPreComment(&contents);

  std::set<int> public_dependencies;
  std::set<int> weak_dependencies;
  public_dependencies.insert(public_dependencies_,
                             public_dependencies_ + public_dependency_count_);
  weak_dependencies.insert(weak_dependencies_,
                           weak_dependencies_ + weak_dependency_count_);

  for (int i = 0; i < dependency_count(); i++) {
    if (public_dependencies.count(i) > 0) {
      strings::SubstituteAndAppend(&contents, "import public \"$0\";\n",
                                   dependency(i)->name());
    } else if (weak_dependencies.count(i) > 0) {
      strings::SubstituteAndAppend(&contents, "import weak \"$0\";\n",
                                   dependency(i)->name());
    } else {
      strings::SubstituteAndAppend(&contents, "import \"$0\";\n",
                                   dependency(i)->name());
    }
  }

  if (!package().empty()) {
    std::vector<int> path;
    path.push_back(FileDescriptorProto::kPackageFieldNumber);
    SourceLocationCommentPrinter package_comment(this, path, "",
                                                 debug_string_options);
    package_comment.AddPreComment(&contents);
    strings::SubstituteAndAppend(&contents, "package $0;\n\n", package());
    package_comment.AddPostComment(&contents);
  }

  if (FormatLineOptions(0, options(), pool(), &contents)) {
    contents.append("\n");  // add some space if we had options
  }

  for (int i = 0; i < enum_type_count(); i++) {
    enum_type(i)->DebugString(0, &contents, debug_string_options);
    contents.append("\n");
  }

  // Find all the 'group' type extensions; we will not output their nested
  // definitions (those will be done with their group field descriptor).
  std::set<const Descriptor*> groups;
  for (int i = 0; i < extension_count(); i++) {
    if (extension(i)->type() == FieldDescriptor::TYPE_GROUP) {
      groups.insert(extension(i)->message_type());
    }
  }

  for (int i = 0; i < message_type_count(); i++) {
    if (groups.count(message_type(i)) == 0) {
      message_type(i)->DebugString(0, &contents, debug_string_options,
                                   /* include_opening_clause */ true);
      contents.append("\n");
    }
  }

  for (int i = 0; i < service_count(); i++) {
    service(i)->DebugString(&contents, debug_string_options);
    contents.append("\n");
  }

  const Descriptor* containing_type = nullptr;
  for (int i = 0; i < extension_count(); i++) {
    if (extension(i)->containing_type() != containing_type) {
      if (i > 0) contents.append("}\n\n");
      containing_type = extension(i)->containing_type();
      strings::SubstituteAndAppend(&contents, "extend .$0 {\n",
                                   containing_type->full_name());
    }
    extension(i)->DebugString(1, FieldDescriptor::PRINT_LABEL, &contents,
                              debug_string_options);
  }
  if (extension_count() > 0) contents.append("}\n\n");

  comment_printer.AddPostComment(&contents);

  return contents;
}

std::string Descriptor::DebugString() const {
  DebugStringOptions options;  // default options
  return DebugStringWithOptions(options);
}

std::string Descriptor::DebugStringWithOptions(
    const DebugStringOptions& options) const {
  std::string contents;
  DebugString(0, &contents, options, /* include_opening_clause */ true);
  return contents;
}

void Descriptor::DebugString(int depth, std::string* contents,
                             const DebugStringOptions& debug_string_options,
                             bool include_opening_clause) const {
  if (options().map_entry()) {
    // Do not generate debug string for auto-generated map-entry type.
    return;
  }
  std::string prefix(depth * 2, ' ');
  ++depth;

  SourceLocationCommentPrinter comment_printer(this, prefix,
                                               debug_string_options);
  comment_printer.AddPreComment(contents);

  if (include_opening_clause) {
    strings::SubstituteAndAppend(contents, "$0message $1", prefix, name());
  }
  contents->append(" {\n");

  FormatLineOptions(depth, options(), file()->pool(), contents);

  // Find all the 'group' types for fields and extensions; we will not output
  // their nested definitions (those will be done with their group field
  // descriptor).
  std::set<const Descriptor*> groups;
  for (int i = 0; i < field_count(); i++) {
    if (field(i)->type() == FieldDescriptor::TYPE_GROUP) {
      groups.insert(field(i)->message_type());
    }
  }
  for (int i = 0; i < extension_count(); i++) {
    if (extension(i)->type() == FieldDescriptor::TYPE_GROUP) {
      groups.insert(extension(i)->message_type());
    }
  }

  for (int i = 0; i < nested_type_count(); i++) {
    if (groups.count(nested_type(i)) == 0) {
      nested_type(i)->DebugString(depth, contents, debug_string_options,
                                  /* include_opening_clause */ true);
    }
  }
  for (int i = 0; i < enum_type_count(); i++) {
    enum_type(i)->DebugString(depth, contents, debug_string_options);
  }
  for (int i = 0; i < field_count(); i++) {
    if (field(i)->containing_oneof() == nullptr) {
      field(i)->DebugString(depth, FieldDescriptor::PRINT_LABEL, contents,
                            debug_string_options);
    } else if (field(i)->containing_oneof()->field(0) == field(i)) {
      // This is the first field in this oneof, so print the whole oneof.
      field(i)->containing_oneof()->DebugString(depth, contents,
                                                debug_string_options);
    }
  }

  for (int i = 0; i < extension_range_count(); i++) {
    strings::SubstituteAndAppend(contents, "$0  extensions $1 to $2;\n", prefix,
                                 extension_range(i)->start,
                                 extension_range(i)->end - 1);
  }

  // Group extensions by what they extend, so they can be printed out together.
  const Descriptor* containing_type = nullptr;
  for (int i = 0; i < extension_count(); i++) {
    if (extension(i)->containing_type() != containing_type) {
      if (i > 0) strings::SubstituteAndAppend(contents, "$0  }\n", prefix);
      containing_type = extension(i)->containing_type();
      strings::SubstituteAndAppend(contents, "$0  extend .$1 {\n", prefix,
                                   containing_type->full_name());
    }
    extension(i)->DebugString(depth + 1, FieldDescriptor::PRINT_LABEL, contents,
                              debug_string_options);
  }
  if (extension_count() > 0)
    strings::SubstituteAndAppend(contents, "$0  }\n", prefix);

  if (reserved_range_count() > 0) {
    strings::SubstituteAndAppend(contents, "$0  reserved ", prefix);
    for (int i = 0; i < reserved_range_count(); i++) {
      const Descriptor::ReservedRange* range = reserved_range(i);
      if (range->end == range->start + 1) {
        strings::SubstituteAndAppend(contents, "$0, ", range->start);
      } else {
        strings::SubstituteAndAppend(contents, "$0 to $1, ", range->start,
                                     range->end - 1);
      }
    }
    contents->replace(contents->size() - 2, 2, ";\n");
  }

  if (reserved_name_count() > 0) {
    strings::SubstituteAndAppend(contents, "$0  reserved ", prefix);
    for (int i = 0; i < reserved_name_count(); i++) {
      strings::SubstituteAndAppend(contents, "\"$0\", ",
                                   CEscape(reserved_name(i)));
    }
    contents->replace(contents->size() - 2, 2, ";\n");
  }

  strings::SubstituteAndAppend(contents, "$0}\n", prefix);
  comment_printer.AddPostComment(contents);
}

std::string FieldDescriptor::DebugString() const {
  DebugStringOptions options;  // default options
  return DebugStringWithOptions(options);
}

std::string FieldDescriptor::DebugStringWithOptions(
    const DebugStringOptions& debug_string_options) const {
  std::string contents;
  int depth = 0;
  if (is_extension()) {
    strings::SubstituteAndAppend(&contents, "extend .$0 {\n",
                                 containing_type()->full_name());
    depth = 1;
  }
  DebugString(depth, PRINT_LABEL, &contents, debug_string_options);
  if (is_extension()) {
    contents.append("}\n");
  }
  return contents;
}

// The field type string used in FieldDescriptor::DebugString()
std::string FieldDescriptor::FieldTypeNameDebugString() const {
  switch (type()) {
    case TYPE_MESSAGE:
      return "." + message_type()->full_name();
    case TYPE_ENUM:
      return "." + enum_type()->full_name();
    default:
      return kTypeToName[type()];
  }
}

void FieldDescriptor::DebugString(
    int depth, PrintLabelFlag print_label_flag, std::string* contents,
    const DebugStringOptions& debug_string_options) const {
  std::string prefix(depth * 2, ' ');
  std::string field_type;

  // Special case map fields.
  if (is_map()) {
    strings::SubstituteAndAppend(
        &field_type, "map<$0, $1>",
        message_type()->field(0)->FieldTypeNameDebugString(),
        message_type()->field(1)->FieldTypeNameDebugString());
  } else {
    field_type = FieldTypeNameDebugString();
  }

  bool print_label = true;
  // Determine whether to omit label:
  //   1. For an optional field, omit label if it's in oneof or in proto3.
  //   2. For a repeated field, omit label if it's a map.
  if (is_optional() && (print_label_flag == OMIT_LABEL ||
                        file()->syntax() == FileDescriptor::SYNTAX_PROTO3)) {
    print_label = false;
  } else if (is_map()) {
    print_label = false;
  }
  std::string label;
  if (print_label) {
    label = kLabelToName[this->label()];
    label.push_back(' ');
  }

  SourceLocationCommentPrinter comment_printer(this, prefix,
                                               debug_string_options);
  comment_printer.AddPreComment(contents);

  strings::SubstituteAndAppend(
      contents, "$0$1$2 $3 = $4", prefix, label, field_type,
      type() == TYPE_GROUP ? message_type()->name() : name(), number());

  bool bracketed = false;
  if (has_default_value()) {
    bracketed = true;
    strings::SubstituteAndAppend(contents, " [default = $0",
                                 DefaultValueAsString(true));
  }
  if (has_json_name_) {
    if (!bracketed) {
      bracketed = true;
      contents->append("[");
    } else {
      contents->append(", ");
    }
    contents->append("json_name = \"");
    contents->append(CEscape(json_name()));
    contents->append("\"");
  }

  std::string formatted_options;
  if (FormatBracketedOptions(depth, options(), file()->pool(),
                             &formatted_options)) {
    contents->append(bracketed ? ", " : " [");
    bracketed = true;
    contents->append(formatted_options);
  }

  if (bracketed) {
    contents->append("]");
  }

  if (type() == TYPE_GROUP) {
    if (debug_string_options.elide_group_body) {
      contents->append(" { ... };\n");
    } else {
      message_type()->DebugString(depth, contents, debug_string_options,
                                  /* include_opening_clause */ false);
    }
  } else {
    contents->append(";\n");
  }

  comment_printer.AddPostComment(contents);
}

std::string OneofDescriptor::DebugString() const {
  DebugStringOptions options;  // default values
  return DebugStringWithOptions(options);
}

std::string OneofDescriptor::DebugStringWithOptions(
    const DebugStringOptions& options) const {
  std::string contents;
  DebugString(0, &contents, options);
  return contents;
}

void OneofDescriptor::DebugString(
    int depth, std::string* contents,
    const DebugStringOptions& debug_string_options) const {
  std::string prefix(depth * 2, ' ');
  ++depth;
  SourceLocationCommentPrinter comment_printer(this, prefix,
                                               debug_string_options);
  comment_printer.AddPreComment(contents);
  strings::SubstituteAndAppend(contents, "$0oneof $1 {", prefix, name());

  FormatLineOptions(depth, options(), containing_type()->file()->pool(),
                    contents);

  if (debug_string_options.elide_oneof_body) {
    contents->append(" ... }\n");
  } else {
    contents->append("\n");
    for (int i = 0; i < field_count(); i++) {
      field(i)->DebugString(depth, FieldDescriptor::OMIT_LABEL, contents,
                            debug_string_options);
    }
    strings::SubstituteAndAppend(contents, "$0}\n", prefix);
  }
  comment_printer.AddPostComment(contents);
}

std::string EnumDescriptor::DebugString() const {
  DebugStringOptions options;  // default values
  return DebugStringWithOptions(options);
}

std::string EnumDescriptor::DebugStringWithOptions(
    const DebugStringOptions& options) const {
  std::string contents;
  DebugString(0, &contents, options);
  return contents;
}

void EnumDescriptor::DebugString(
    int depth, std::string* contents,
    const DebugStringOptions& debug_string_options) const {
  std::string prefix(depth * 2, ' ');
  ++depth;

  SourceLocationCommentPrinter comment_printer(this, prefix,
                                               debug_string_options);
  comment_printer.AddPreComment(contents);

  strings::SubstituteAndAppend(contents, "$0enum $1 {\n", prefix, name());

  FormatLineOptions(depth, options(), file()->pool(), contents);

  for (int i = 0; i < value_count(); i++) {
    value(i)->DebugString(depth, contents, debug_string_options);
  }

  if (reserved_range_count() > 0) {
    strings::SubstituteAndAppend(contents, "$0  reserved ", prefix);
    for (int i = 0; i < reserved_range_count(); i++) {
      const EnumDescriptor::ReservedRange* range = reserved_range(i);
      if (range->end == range->start) {
        strings::SubstituteAndAppend(contents, "$0, ", range->start);
      } else {
        strings::SubstituteAndAppend(contents, "$0 to $1, ", range->start,
                                     range->end);
      }
    }
    contents->replace(contents->size() - 2, 2, ";\n");
  }

  if (reserved_name_count() > 0) {
    strings::SubstituteAndAppend(contents, "$0  reserved ", prefix);
    for (int i = 0; i < reserved_name_count(); i++) {
      strings::SubstituteAndAppend(contents, "\"$0\", ",
                                   CEscape(reserved_name(i)));
    }
    contents->replace(contents->size() - 2, 2, ";\n");
  }

  strings::SubstituteAndAppend(contents, "$0}\n", prefix);

  comment_printer.AddPostComment(contents);
}

std::string EnumValueDescriptor::DebugString() const {
  DebugStringOptions options;  // default values
  return DebugStringWithOptions(options);
}

std::string EnumValueDescriptor::DebugStringWithOptions(
    const DebugStringOptions& options) const {
  std::string contents;
  DebugString(0, &contents, options);
  return contents;
}

void EnumValueDescriptor::DebugString(
    int depth, std::string* contents,
    const DebugStringOptions& debug_string_options) const {
  std::string prefix(depth * 2, ' ');

  SourceLocationCommentPrinter comment_printer(this, prefix,
                                               debug_string_options);
  comment_printer.AddPreComment(contents);

  strings::SubstituteAndAppend(contents, "$0$1 = $2", prefix, name(), number());

  std::string formatted_options;
  if (FormatBracketedOptions(depth, options(), type()->file()->pool(),
                             &formatted_options)) {
    strings::SubstituteAndAppend(contents, " [$0]", formatted_options);
  }
  contents->append(";\n");

  comment_printer.AddPostComment(contents);
}

std::string ServiceDescriptor::DebugString() const {
  DebugStringOptions options;  // default values
  return DebugStringWithOptions(options);
}

std::string ServiceDescriptor::DebugStringWithOptions(
    const DebugStringOptions& options) const {
  std::string contents;
  DebugString(&contents, options);
  return contents;
}

void ServiceDescriptor::DebugString(
    std::string* contents,
    const DebugStringOptions& debug_string_options) const {
  SourceLocationCommentPrinter comment_printer(this, /* prefix */ "",
                                               debug_string_options);
  comment_printer.AddPreComment(contents);

  strings::SubstituteAndAppend(contents, "service $0 {\n", name());

  FormatLineOptions(1, options(), file()->pool(), contents);

  for (int i = 0; i < method_count(); i++) {
    method(i)->DebugString(1, contents, debug_string_options);
  }

  contents->append("}\n");

  comment_printer.AddPostComment(contents);
}

std::string MethodDescriptor::DebugString() const {
  DebugStringOptions options;  // default values
  return DebugStringWithOptions(options);
}

std::string MethodDescriptor::DebugStringWithOptions(
    const DebugStringOptions& options) const {
  std::string contents;
  DebugString(0, &contents, options);
  return contents;
}

void MethodDescriptor::DebugString(
    int depth, std::string* contents,
    const DebugStringOptions& debug_string_options) const {
  std::string prefix(depth * 2, ' ');
  ++depth;

  SourceLocationCommentPrinter comment_printer(this, prefix,
                                               debug_string_options);
  comment_printer.AddPreComment(contents);

  strings::SubstituteAndAppend(
      contents, "$0rpc $1($4.$2) returns ($5.$3)", prefix, name(),
      input_type()->full_name(), output_type()->full_name(),
      client_streaming() ? "stream " : "", server_streaming() ? "stream " : "");

  std::string formatted_options;
  if (FormatLineOptions(depth, options(), service()->file()->pool(),
                        &formatted_options)) {
    strings::SubstituteAndAppend(contents, " {\n$0$1}\n", formatted_options,
                                 prefix);
  } else {
    contents->append(";\n");
  }

  comment_printer.AddPostComment(contents);
}


// Location methods ===============================================

bool FileDescriptor::GetSourceLocation(const std::vector<int>& path,
                                       SourceLocation* out_location) const {
  GOOGLE_CHECK(out_location != nullptr);
  if (source_code_info_) {
    if (const SourceCodeInfo_Location* loc =
            tables_->GetSourceLocation(path, source_code_info_)) {
      const RepeatedField<int32>& span = loc->span();
      if (span.size() == 3 || span.size() == 4) {
        out_location->start_line = span.Get(0);
        out_location->start_column = span.Get(1);
        out_location->end_line = span.Get(span.size() == 3 ? 0 : 2);
        out_location->end_column = span.Get(span.size() - 1);

        out_location->leading_comments = loc->leading_comments();
        out_location->trailing_comments = loc->trailing_comments();
        out_location->leading_detached_comments.assign(
            loc->leading_detached_comments().begin(),
            loc->leading_detached_comments().end());
        return true;
      }
    }
  }
  return false;
}

bool FileDescriptor::GetSourceLocation(SourceLocation* out_location) const {
  std::vector<int> path;  // empty path for root FileDescriptor
  return GetSourceLocation(path, out_location);
}

bool FieldDescriptor::is_packed() const {
  if (!is_packable()) return false;
  if (file_->syntax() == FileDescriptor::SYNTAX_PROTO2) {
    return (options_ != nullptr) && options_->packed();
  } else {
    return options_ == nullptr || !options_->has_packed() || options_->packed();
  }
}

bool Descriptor::GetSourceLocation(SourceLocation* out_location) const {
  std::vector<int> path;
  GetLocationPath(&path);
  return file()->GetSourceLocation(path, out_location);
}

bool FieldDescriptor::GetSourceLocation(SourceLocation* out_location) const {
  std::vector<int> path;
  GetLocationPath(&path);
  return file()->GetSourceLocation(path, out_location);
}

bool OneofDescriptor::GetSourceLocation(SourceLocation* out_location) const {
  std::vector<int> path;
  GetLocationPath(&path);
  return containing_type()->file()->GetSourceLocation(path, out_location);
}

bool EnumDescriptor::GetSourceLocation(SourceLocation* out_location) const {
  std::vector<int> path;
  GetLocationPath(&path);
  return file()->GetSourceLocation(path, out_location);
}

bool MethodDescriptor::GetSourceLocation(SourceLocation* out_location) const {
  std::vector<int> path;
  GetLocationPath(&path);
  return service()->file()->GetSourceLocation(path, out_location);
}

bool ServiceDescriptor::GetSourceLocation(SourceLocation* out_location) const {
  std::vector<int> path;
  GetLocationPath(&path);
  return file()->GetSourceLocation(path, out_location);
}

bool EnumValueDescriptor::GetSourceLocation(
    SourceLocation* out_location) const {
  std::vector<int> path;
  GetLocationPath(&path);
  return type()->file()->GetSourceLocation(path, out_location);
}

void Descriptor::GetLocationPath(std::vector<int>* output) const {
  if (containing_type()) {
    containing_type()->GetLocationPath(output);
    output->push_back(DescriptorProto::kNestedTypeFieldNumber);
    output->push_back(index());
  } else {
    output->push_back(FileDescriptorProto::kMessageTypeFieldNumber);
    output->push_back(index());
  }
}

void FieldDescriptor::GetLocationPath(std::vector<int>* output) const {
  if (is_extension()) {
    if (extension_scope() == nullptr) {
      output->push_back(FileDescriptorProto::kExtensionFieldNumber);
      output->push_back(index());
    } else {
      extension_scope()->GetLocationPath(output);
      output->push_back(DescriptorProto::kExtensionFieldNumber);
      output->push_back(index());
    }
  } else {
    containing_type()->GetLocationPath(output);
    output->push_back(DescriptorProto::kFieldFieldNumber);
    output->push_back(index());
  }
}

void OneofDescriptor::GetLocationPath(std::vector<int>* output) const {
  containing_type()->GetLocationPath(output);
  output->push_back(DescriptorProto::kOneofDeclFieldNumber);
  output->push_back(index());
}

void EnumDescriptor::GetLocationPath(std::vector<int>* output) const {
  if (containing_type()) {
    containing_type()->GetLocationPath(output);
    output->push_back(DescriptorProto::kEnumTypeFieldNumber);
    output->push_back(index());
  } else {
    output->push_back(FileDescriptorProto::kEnumTypeFieldNumber);
    output->push_back(index());
  }
}

void EnumValueDescriptor::GetLocationPath(std::vector<int>* output) const {
  type()->GetLocationPath(output);
  output->push_back(EnumDescriptorProto::kValueFieldNumber);
  output->push_back(index());
}

void ServiceDescriptor::GetLocationPath(std::vector<int>* output) const {
  output->push_back(FileDescriptorProto::kServiceFieldNumber);
  output->push_back(index());
}

void MethodDescriptor::GetLocationPath(std::vector<int>* output) const {
  service()->GetLocationPath(output);
  output->push_back(ServiceDescriptorProto::kMethodFieldNumber);
  output->push_back(index());
}

// ===================================================================

namespace {

// Represents an options message to interpret. Extension names in the option
// name are resolved relative to name_scope. element_name and orig_opt are
// used only for error reporting (since the parser records locations against
// pointers in the original options, not the mutable copy). The Message must be
// one of the Options messages in descriptor.proto.
struct OptionsToInterpret {
  OptionsToInterpret(const std::string& ns, const std::string& el,
                     const std::vector<int>& path, const Message* orig_opt,
                     Message* opt)
      : name_scope(ns),
        element_name(el),
        element_path(path),
        original_options(orig_opt),
        options(opt) {}
  std::string name_scope;
  std::string element_name;
  std::vector<int> element_path;
  const Message* original_options;
  Message* options;
};

}  // namespace

class DescriptorBuilder {
 public:
  DescriptorBuilder(const DescriptorPool* pool, DescriptorPool::Tables* tables,
                    DescriptorPool::ErrorCollector* error_collector);
  ~DescriptorBuilder();

  const FileDescriptor* BuildFile(const FileDescriptorProto& proto);

 private:
  friend class OptionInterpreter;

  // Non-recursive part of BuildFile functionality.
  FileDescriptor* BuildFileImpl(const FileDescriptorProto& proto);

  const DescriptorPool* pool_;
  DescriptorPool::Tables* tables_;  // for convenience
  DescriptorPool::ErrorCollector* error_collector_;

  // As we build descriptors we store copies of the options messages in
  // them. We put pointers to those copies in this vector, as we build, so we
  // can later (after cross-linking) interpret those options.
  std::vector<OptionsToInterpret> options_to_interpret_;

  bool had_errors_;
  std::string filename_;
  FileDescriptor* file_;
  FileDescriptorTables* file_tables_;
  std::set<const FileDescriptor*> dependencies_;

  // unused_dependency_ is used to record the unused imported files.
  // Note: public import is not considered.
  std::set<const FileDescriptor*> unused_dependency_;

  // If LookupSymbol() finds a symbol that is in a file which is not a declared
  // dependency of this file, it will fail, but will set
  // possible_undeclared_dependency_ to point at that file.  This is only used
  // by AddNotDefinedError() to report a more useful error message.
  // possible_undeclared_dependency_name_ is the name of the symbol that was
  // actually found in possible_undeclared_dependency_, which may be a parent
  // of the symbol actually looked for.
  const FileDescriptor* possible_undeclared_dependency_;
  std::string possible_undeclared_dependency_name_;

  // If LookupSymbol() could resolve a symbol which is not defined,
  // record the resolved name.  This is only used by AddNotDefinedError()
  // to report a more useful error message.
  std::string undefine_resolved_name_;

  void AddError(const std::string& element_name, const Message& descriptor,
                DescriptorPool::ErrorCollector::ErrorLocation location,
                const std::string& error);
  void AddError(const std::string& element_name, const Message& descriptor,
                DescriptorPool::ErrorCollector::ErrorLocation location,
                const char* error);
  void AddRecursiveImportError(const FileDescriptorProto& proto, int from_here);
  void AddTwiceListedError(const FileDescriptorProto& proto, int index);
  void AddImportError(const FileDescriptorProto& proto, int index);

  // Adds an error indicating that undefined_symbol was not defined.  Must
  // only be called after LookupSymbol() fails.
  void AddNotDefinedError(
      const std::string& element_name, const Message& descriptor,
      DescriptorPool::ErrorCollector::ErrorLocation location,
      const std::string& undefined_symbol);

  void AddWarning(const std::string& element_name, const Message& descriptor,
                  DescriptorPool::ErrorCollector::ErrorLocation location,
                  const std::string& error);

  // Silly helper which determines if the given file is in the given package.
  // I.e., either file->package() == package_name or file->package() is a
  // nested package within package_name.
  bool IsInPackage(const FileDescriptor* file, const std::string& package_name);

  // Helper function which finds all public dependencies of the given file, and
  // stores the them in the dependencies_ set in the builder.
  void RecordPublicDependencies(const FileDescriptor* file);

  // Like tables_->FindSymbol(), but additionally:
  // - Search the pool's underlay if not found in tables_.
  // - Insure that the resulting Symbol is from one of the file's declared
  //   dependencies.
  Symbol FindSymbol(const std::string& name, bool build_it = true);

  // Like FindSymbol() but does not require that the symbol is in one of the
  // file's declared dependencies.
  Symbol FindSymbolNotEnforcingDeps(const std::string& name,
                                    bool build_it = true);

  // This implements the body of FindSymbolNotEnforcingDeps().
  Symbol FindSymbolNotEnforcingDepsHelper(const DescriptorPool* pool,
                                          const std::string& name,
                                          bool build_it = true);

  // Like FindSymbol(), but looks up the name relative to some other symbol
  // name.  This first searches siblings of relative_to, then siblings of its
  // parents, etc.  For example, LookupSymbol("foo.bar", "baz.qux.corge") makes
  // the following calls, returning the first non-null result:
  // FindSymbol("baz.qux.foo.bar"), FindSymbol("baz.foo.bar"),
  // FindSymbol("foo.bar").  If AllowUnknownDependencies() has been called
  // on the DescriptorPool, this will generate a placeholder type if
  // the name is not found (unless the name itself is malformed).  The
  // placeholder_type parameter indicates what kind of placeholder should be
  // constructed in this case.  The resolve_mode parameter determines whether
  // any symbol is returned, or only symbols that are types.  Note, however,
  // that LookupSymbol may still return a non-type symbol in LOOKUP_TYPES mode,
  // if it believes that's all it could refer to.  The caller should always
  // check that it receives the type of symbol it was expecting.
  enum ResolveMode { LOOKUP_ALL, LOOKUP_TYPES };
  Symbol LookupSymbol(const std::string& name, const std::string& relative_to,
                      DescriptorPool::PlaceholderType placeholder_type =
                          DescriptorPool::PLACEHOLDER_MESSAGE,
                      ResolveMode resolve_mode = LOOKUP_ALL,
                      bool build_it = true);

  // Like LookupSymbol() but will not return a placeholder even if
  // AllowUnknownDependencies() has been used.
  Symbol LookupSymbolNoPlaceholder(const std::string& name,
                                   const std::string& relative_to,
                                   ResolveMode resolve_mode = LOOKUP_ALL,
                                   bool build_it = true);

  // Calls tables_->AddSymbol() and records an error if it fails.  Returns
  // true if successful or false if failed, though most callers can ignore
  // the return value since an error has already been recorded.
  bool AddSymbol(const std::string& full_name, const void* parent,
                 const std::string& name, const Message& proto, Symbol symbol);

  // Like AddSymbol(), but succeeds if the symbol is already defined as long
  // as the existing definition is also a package (because it's OK to define
  // the same package in two different files).  Also adds all parents of the
  // packgae to the symbol table (e.g. AddPackage("foo.bar", ...) will add
  // "foo.bar" and "foo" to the table).
  void AddPackage(const std::string& name, const Message& proto,
                  const FileDescriptor* file);

  // Checks that the symbol name contains only alphanumeric characters and
  // underscores.  Records an error otherwise.
  void ValidateSymbolName(const std::string& name, const std::string& full_name,
                          const Message& proto);

  // Used by BUILD_ARRAY macro (below) to avoid having to have the type
  // specified as a macro parameter.
  template <typename Type>
  inline void AllocateArray(int size, Type** output) {
    *output = tables_->AllocateArray<Type>(size);
  }

  // Allocates a copy of orig_options in tables_ and stores it in the
  // descriptor. Remembers its uninterpreted options, to be interpreted
  // later. DescriptorT must be one of the Descriptor messages from
  // descriptor.proto.
  template <class DescriptorT>
  void AllocateOptions(const typename DescriptorT::OptionsType& orig_options,
                       DescriptorT* descriptor, int options_field_tag);
  // Specialization for FileOptions.
  void AllocateOptions(const FileOptions& orig_options,
                       FileDescriptor* descriptor);

  // Implementation for AllocateOptions(). Don't call this directly.
  template <class DescriptorT>
  void AllocateOptionsImpl(
      const std::string& name_scope, const std::string& element_name,
      const typename DescriptorT::OptionsType& orig_options,
      DescriptorT* descriptor, const std::vector<int>& options_path);

  // Allocate string on the string pool and initialize it to full proto name.
  // Full proto name is "scope.proto_name" if scope is non-empty and
  // "proto_name" otherwise.
  std::string* AllocateNameString(const std::string& scope,
                                  const std::string& proto_name);

  // These methods all have the same signature for the sake of the BUILD_ARRAY
  // macro, below.
  void BuildMessage(const DescriptorProto& proto, const Descriptor* parent,
                    Descriptor* result);
  void BuildFieldOrExtension(const FieldDescriptorProto& proto,
                             const Descriptor* parent, FieldDescriptor* result,
                             bool is_extension);
  void BuildField(const FieldDescriptorProto& proto, const Descriptor* parent,
                  FieldDescriptor* result) {
    BuildFieldOrExtension(proto, parent, result, false);
  }
  void BuildExtension(const FieldDescriptorProto& proto,
                      const Descriptor* parent, FieldDescriptor* result) {
    BuildFieldOrExtension(proto, parent, result, true);
  }
  void BuildExtensionRange(const DescriptorProto::ExtensionRange& proto,
                           const Descriptor* parent,
                           Descriptor::ExtensionRange* result);
  void BuildReservedRange(const DescriptorProto::ReservedRange& proto,
                          const Descriptor* parent,
                          Descriptor::ReservedRange* result);
  void BuildReservedRange(const EnumDescriptorProto::EnumReservedRange& proto,
                          const EnumDescriptor* parent,
                          EnumDescriptor::ReservedRange* result);
  void BuildOneof(const OneofDescriptorProto& proto, Descriptor* parent,
                  OneofDescriptor* result);
  void CheckEnumValueUniqueness(const EnumDescriptorProto& proto,
                                const EnumDescriptor* result);
  void BuildEnum(const EnumDescriptorProto& proto, const Descriptor* parent,
                 EnumDescriptor* result);
  void BuildEnumValue(const EnumValueDescriptorProto& proto,
                      const EnumDescriptor* parent,
                      EnumValueDescriptor* result);
  void BuildService(const ServiceDescriptorProto& proto, const void* dummy,
                    ServiceDescriptor* result);
  void BuildMethod(const MethodDescriptorProto& proto,
                   const ServiceDescriptor* parent, MethodDescriptor* result);

  void LogUnusedDependency(const FileDescriptorProto& proto,
                           const FileDescriptor* result);

  // Must be run only after building.
  //
  // NOTE: Options will not be available during cross-linking, as they
  // have not yet been interpreted. Defer any handling of options to the
  // Validate*Options methods.
  void CrossLinkFile(FileDescriptor* file, const FileDescriptorProto& proto);
  void CrossLinkMessage(Descriptor* message, const DescriptorProto& proto);
  void CrossLinkField(FieldDescriptor* field,
                      const FieldDescriptorProto& proto);
  void CrossLinkExtensionRange(Descriptor::ExtensionRange* range,
                               const DescriptorProto::ExtensionRange& proto);
  void CrossLinkEnum(EnumDescriptor* enum_type,
                     const EnumDescriptorProto& proto);
  void CrossLinkEnumValue(EnumValueDescriptor* enum_value,
                          const EnumValueDescriptorProto& proto);
  void CrossLinkService(ServiceDescriptor* service,
                        const ServiceDescriptorProto& proto);
  void CrossLinkMethod(MethodDescriptor* method,
                       const MethodDescriptorProto& proto);

  // Must be run only after cross-linking.
  void InterpretOptions();

  // A helper class for interpreting options.
  class OptionInterpreter {
   public:
    // Creates an interpreter that operates in the context of the pool of the
    // specified builder, which must not be nullptr. We don't take ownership of
    // the builder.
    explicit OptionInterpreter(DescriptorBuilder* builder);

    ~OptionInterpreter();

    // Interprets the uninterpreted options in the specified Options message.
    // On error, calls AddError() on the underlying builder and returns false.
    // Otherwise returns true.
    bool InterpretOptions(OptionsToInterpret* options_to_interpret);

    // Updates the given source code info by re-writing uninterpreted option
    // locations to refer to the corresponding interpreted option.
    void UpdateSourceCodeInfo(SourceCodeInfo* info);

    class AggregateOptionFinder;

   private:
    // Interprets uninterpreted_option_ on the specified message, which
    // must be the mutable copy of the original options message to which
    // uninterpreted_option_ belongs. The given src_path is the source
    // location path to the uninterpreted option, and options_path is the
    // source location path to the options message. The location paths are
    // recorded and then used in UpdateSourceCodeInfo.
    bool InterpretSingleOption(Message* options,
                               const std::vector<int>& src_path,
                               const std::vector<int>& options_path);

    // Adds the uninterpreted_option to the given options message verbatim.
    // Used when AllowUnknownDependencies() is in effect and we can't find
    // the option's definition.
    void AddWithoutInterpreting(const UninterpretedOption& uninterpreted_option,
                                Message* options);

    // A recursive helper function that drills into the intermediate fields
    // in unknown_fields to check if field innermost_field is set on the
    // innermost message. Returns false and sets an error if so.
    bool ExamineIfOptionIsSet(
        std::vector<const FieldDescriptor*>::const_iterator
            intermediate_fields_iter,
        std::vector<const FieldDescriptor*>::const_iterator
            intermediate_fields_end,
        const FieldDescriptor* innermost_field,
        const std::string& debug_msg_name,
        const UnknownFieldSet& unknown_fields);

    // Validates the value for the option field of the currently interpreted
    // option and then sets it on the unknown_field.
    bool SetOptionValue(const FieldDescriptor* option_field,
                        UnknownFieldSet* unknown_fields);

    // Parses an aggregate value for a CPPTYPE_MESSAGE option and
    // saves it into *unknown_fields.
    bool SetAggregateOption(const FieldDescriptor* option_field,
                            UnknownFieldSet* unknown_fields);

    // Convenience functions to set an int field the right way, depending on
    // its wire type (a single int CppType can represent multiple wire types).
    void SetInt32(int number, int32 value, FieldDescriptor::Type type,
                  UnknownFieldSet* unknown_fields);
    void SetInt64(int number, int64 value, FieldDescriptor::Type type,
                  UnknownFieldSet* unknown_fields);
    void SetUInt32(int number, uint32 value, FieldDescriptor::Type type,
                   UnknownFieldSet* unknown_fields);
    void SetUInt64(int number, uint64 value, FieldDescriptor::Type type,
                   UnknownFieldSet* unknown_fields);

    // A helper function that adds an error at the specified location of the
    // option we're currently interpreting, and returns false.
    bool AddOptionError(DescriptorPool::ErrorCollector::ErrorLocation location,
                        const std::string& msg) {
      builder_->AddError(options_to_interpret_->element_name,
                         *uninterpreted_option_, location, msg);
      return false;
    }

    // A helper function that adds an error at the location of the option name
    // and returns false.
    bool AddNameError(const std::string& msg) {
      return AddOptionError(DescriptorPool::ErrorCollector::OPTION_NAME, msg);
    }

    // A helper function that adds an error at the location of the option name
    // and returns false.
    bool AddValueError(const std::string& msg) {
      return AddOptionError(DescriptorPool::ErrorCollector::OPTION_VALUE, msg);
    }

    // We interpret against this builder's pool. Is never nullptr. We don't own
    // this pointer.
    DescriptorBuilder* builder_;

    // The options we're currently interpreting, or nullptr if we're not in a
    // call to InterpretOptions.
    const OptionsToInterpret* options_to_interpret_;

    // The option we're currently interpreting within options_to_interpret_, or
    // nullptr if we're not in a call to InterpretOptions(). This points to a
    // submessage of the original option, not the mutable copy. Therefore we
    // can use it to find locations recorded by the parser.
    const UninterpretedOption* uninterpreted_option_;

    // This maps the element path of uninterpreted options to the element path
    // of the resulting interpreted option. This is used to modify a file's
    // source code info to account for option interpretation.
    std::map<std::vector<int>, std::vector<int>> interpreted_paths_;

    // This maps the path to a repeated option field to the known number of
    // elements the field contains. This is used to track the compute the
    // index portion of the element path when interpreting a single option.
    std::map<std::vector<int>, int> repeated_option_counts_;

    // Factory used to create the dynamic messages we need to parse
    // any aggregate option values we encounter.
    DynamicMessageFactory dynamic_factory_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(OptionInterpreter);
  };

  // Work-around for broken compilers:  According to the C++ standard,
  // OptionInterpreter should have access to the private members of any class
  // which has declared DescriptorBuilder as a friend.  Unfortunately some old
  // versions of GCC and other compilers do not implement this correctly.  So,
  // we have to have these intermediate methods to provide access.  We also
  // redundantly declare OptionInterpreter a friend just to make things extra
  // clear for these bad compilers.
  friend class OptionInterpreter;
  friend class OptionInterpreter::AggregateOptionFinder;

  static inline bool get_allow_unknown(const DescriptorPool* pool) {
    return pool->allow_unknown_;
  }
  static inline bool get_enforce_weak(const DescriptorPool* pool) {
    return pool->enforce_weak_;
  }
  static inline bool get_is_placeholder(const Descriptor* descriptor) {
    return descriptor->is_placeholder_;
  }
  static inline void assert_mutex_held(const DescriptorPool* pool) {
    if (pool->mutex_ != nullptr) {
      pool->mutex_->AssertHeld();
    }
  }

  // Must be run only after options have been interpreted.
  //
  // NOTE: Validation code must only reference the options in the mutable
  // descriptors, which are the ones that have been interpreted. The const
  // proto references are passed in only so they can be provided to calls to
  // AddError(). Do not look at their options, which have not been interpreted.
  void ValidateFileOptions(FileDescriptor* file,
                           const FileDescriptorProto& proto);
  void ValidateMessageOptions(Descriptor* message,
                              const DescriptorProto& proto);
  void ValidateFieldOptions(FieldDescriptor* field,
                            const FieldDescriptorProto& proto);
  void ValidateEnumOptions(EnumDescriptor* enm,
                           const EnumDescriptorProto& proto);
  void ValidateEnumValueOptions(EnumValueDescriptor* enum_value,
                                const EnumValueDescriptorProto& proto);
  void ValidateServiceOptions(ServiceDescriptor* service,
                              const ServiceDescriptorProto& proto);
  void ValidateMethodOptions(MethodDescriptor* method,
                             const MethodDescriptorProto& proto);
  void ValidateProto3(FileDescriptor* file, const FileDescriptorProto& proto);
  void ValidateProto3Message(Descriptor* message, const DescriptorProto& proto);
  void ValidateProto3Field(FieldDescriptor* field,
                           const FieldDescriptorProto& proto);
  void ValidateProto3Enum(EnumDescriptor* enm,
                          const EnumDescriptorProto& proto);

  // Returns true if the map entry message is compatible with the
  // auto-generated entry message from map fields syntax.
  bool ValidateMapEntry(FieldDescriptor* field,
                        const FieldDescriptorProto& proto);

  // Recursively detects naming conflicts with map entry types for a
  // better error message.
  void DetectMapConflicts(const Descriptor* message,
                          const DescriptorProto& proto);

  void ValidateJSType(FieldDescriptor* field,
                      const FieldDescriptorProto& proto);
};

const FileDescriptor* DescriptorPool::BuildFile(
    const FileDescriptorProto& proto) {
  GOOGLE_CHECK(fallback_database_ == nullptr)
      << "Cannot call BuildFile on a DescriptorPool that uses a "
         "DescriptorDatabase.  You must instead find a way to get your file "
         "into the underlying database.";
  GOOGLE_CHECK(mutex_ == nullptr);  // Implied by the above GOOGLE_CHECK.
  tables_->known_bad_symbols_.clear();
  tables_->known_bad_files_.clear();
  return DescriptorBuilder(this, tables_.get(), nullptr).BuildFile(proto);
}

const FileDescriptor* DescriptorPool::BuildFileCollectingErrors(
    const FileDescriptorProto& proto, ErrorCollector* error_collector) {
  GOOGLE_CHECK(fallback_database_ == nullptr)
      << "Cannot call BuildFile on a DescriptorPool that uses a "
         "DescriptorDatabase.  You must instead find a way to get your file "
         "into the underlying database.";
  GOOGLE_CHECK(mutex_ == nullptr);  // Implied by the above GOOGLE_CHECK.
  tables_->known_bad_symbols_.clear();
  tables_->known_bad_files_.clear();
  return DescriptorBuilder(this, tables_.get(), error_collector)
      .BuildFile(proto);
}

const FileDescriptor* DescriptorPool::BuildFileFromDatabase(
    const FileDescriptorProto& proto) const {
  mutex_->AssertHeld();
  if (tables_->known_bad_files_.count(proto.name()) > 0) {
    return nullptr;
  }
  const FileDescriptor* result =
      DescriptorBuilder(this, tables_.get(), default_error_collector_)
          .BuildFile(proto);
  if (result == nullptr) {
    tables_->known_bad_files_.insert(proto.name());
  }
  return result;
}

DescriptorBuilder::DescriptorBuilder(
    const DescriptorPool* pool, DescriptorPool::Tables* tables,
    DescriptorPool::ErrorCollector* error_collector)
    : pool_(pool),
      tables_(tables),
      error_collector_(error_collector),
      had_errors_(false),
      possible_undeclared_dependency_(nullptr),
      undefine_resolved_name_("") {}

DescriptorBuilder::~DescriptorBuilder() {}

void DescriptorBuilder::AddError(
    const std::string& element_name, const Message& descriptor,
    DescriptorPool::ErrorCollector::ErrorLocation location,
    const std::string& error) {
  if (error_collector_ == nullptr) {
    if (!had_errors_) {
      GOOGLE_LOG(ERROR) << "Invalid proto descriptor for file \"" << filename_
                 << "\":";
    }
    GOOGLE_LOG(ERROR) << "  " << element_name << ": " << error;
  } else {
    error_collector_->AddError(filename_, element_name, &descriptor, location,
                               error);
  }
  had_errors_ = true;
}

void DescriptorBuilder::AddError(
    const std::string& element_name, const Message& descriptor,
    DescriptorPool::ErrorCollector::ErrorLocation location, const char* error) {
  AddError(element_name, descriptor, location, std::string(error));
}

void DescriptorBuilder::AddNotDefinedError(
    const std::string& element_name, const Message& descriptor,
    DescriptorPool::ErrorCollector::ErrorLocation location,
    const std::string& undefined_symbol) {
  if (possible_undeclared_dependency_ == nullptr &&
      undefine_resolved_name_.empty()) {
    AddError(element_name, descriptor, location,
             "\"" + undefined_symbol + "\" is not defined.");
  } else {
    if (possible_undeclared_dependency_ != nullptr) {
      AddError(element_name, descriptor, location,
               "\"" + possible_undeclared_dependency_name_ +
                   "\" seems to be defined in \"" +
                   possible_undeclared_dependency_->name() +
                   "\", which is not "
                   "imported by \"" +
                   filename_ +
                   "\".  To use it here, please "
                   "add the necessary import.");
    }
    if (!undefine_resolved_name_.empty()) {
      AddError(element_name, descriptor, location,
               "\"" + undefined_symbol + "\" is resolved to \"" +
                   undefine_resolved_name_ +
                   "\", which is not defined. "
                   "The innermost scope is searched first in name resolution. "
                   "Consider using a leading '.'(i.e., \"." +
                   undefined_symbol + "\") to start from the outermost scope.");
    }
  }
}

void DescriptorBuilder::AddWarning(
    const std::string& element_name, const Message& descriptor,
    DescriptorPool::ErrorCollector::ErrorLocation location,
    const std::string& error) {
  if (error_collector_ == nullptr) {
    GOOGLE_LOG(WARNING) << filename_ << " " << element_name << ": " << error;
  } else {
    error_collector_->AddWarning(filename_, element_name, &descriptor, location,
                                 error);
  }
}

bool DescriptorBuilder::IsInPackage(const FileDescriptor* file,
                                    const std::string& package_name) {
  return HasPrefixString(file->package(), package_name) &&
         (file->package().size() == package_name.size() ||
          file->package()[package_name.size()] == '.');
}

void DescriptorBuilder::RecordPublicDependencies(const FileDescriptor* file) {
  if (file == nullptr || !dependencies_.insert(file).second) return;
  for (int i = 0; file != nullptr && i < file->public_dependency_count(); i++) {
    RecordPublicDependencies(file->public_dependency(i));
  }
}

Symbol DescriptorBuilder::FindSymbolNotEnforcingDepsHelper(
    const DescriptorPool* pool, const std::string& name, bool build_it) {
  // If we are looking at an underlay, we must lock its mutex_, since we are
  // accessing the underlay's tables_ directly.
  MutexLockMaybe lock((pool == pool_) ? nullptr : pool->mutex_);

  Symbol result = pool->tables_->FindSymbol(name);
  if (result.IsNull() && pool->underlay_ != nullptr) {
    // Symbol not found; check the underlay.
    result = FindSymbolNotEnforcingDepsHelper(pool->underlay_, name);
  }

  if (result.IsNull()) {
    // With lazily_build_dependencies_, a symbol lookup at cross link time is
    // not guaranteed to be successful. In most cases, build_it will be false,
    // which intentionally prevents us from building an import until it's
    // actually needed. In some cases, like registering an extension, we want
    // to build the file containing the symbol, and build_it will be set.
    // Also, build_it will be true when !lazily_build_dependencies_, to provide
    // better error reporting of missing dependencies.
    if (build_it && pool->TryFindSymbolInFallbackDatabase(name)) {
      result = pool->tables_->FindSymbol(name);
    }
  }

  return result;
}

Symbol DescriptorBuilder::FindSymbolNotEnforcingDeps(const std::string& name,
                                                     bool build_it) {
  return FindSymbolNotEnforcingDepsHelper(pool_, name, build_it);
}

Symbol DescriptorBuilder::FindSymbol(const std::string& name, bool build_it) {
  Symbol result = FindSymbolNotEnforcingDeps(name, build_it);

  if (result.IsNull()) return result;

  if (!pool_->enforce_dependencies_) {
    // Hack for CompilerUpgrader, and also used for lazily_build_dependencies_
    return result;
  }

  // Only find symbols which were defined in this file or one of its
  // dependencies.
  const FileDescriptor* file = result.GetFile();
  if (file == file_ || dependencies_.count(file) > 0) {
    unused_dependency_.erase(file);
    return result;
  }

  if (result.type == Symbol::PACKAGE) {
    // Arg, this is overcomplicated.  The symbol is a package name.  It could
    // be that the package was defined in multiple files.  result.GetFile()
    // returns the first file we saw that used this package.  We've determined
    // that that file is not a direct dependency of the file we are currently
    // building, but it could be that some other file which *is* a direct
    // dependency also defines the same package.  We can't really rule out this
    // symbol unless none of the dependencies define it.
    if (IsInPackage(file_, name)) return result;
    for (std::set<const FileDescriptor*>::const_iterator it =
             dependencies_.begin();
         it != dependencies_.end(); ++it) {
      // Note:  A dependency may be nullptr if it was not found or had errors.
      if (*it != nullptr && IsInPackage(*it, name)) return result;
    }
  }

  possible_undeclared_dependency_ = file;
  possible_undeclared_dependency_name_ = name;
  return kNullSymbol;
}

Symbol DescriptorBuilder::LookupSymbolNoPlaceholder(
    const std::string& name, const std::string& relative_to,
    ResolveMode resolve_mode, bool build_it) {
  possible_undeclared_dependency_ = nullptr;
  undefine_resolved_name_.clear();

  if (!name.empty() && name[0] == '.') {
    // Fully-qualified name.
    return FindSymbol(name.substr(1), build_it);
  }

  // If name is something like "Foo.Bar.baz", and symbols named "Foo" are
  // defined in multiple parent scopes, we only want to find "Bar.baz" in the
  // innermost one.  E.g., the following should produce an error:
  //   message Bar { message Baz {} }
  //   message Foo {
  //     message Bar {
  //     }
  //     optional Bar.Baz baz = 1;
  //   }
  // So, we look for just "Foo" first, then look for "Bar.baz" within it if
  // found.
  std::string::size_type name_dot_pos = name.find_first_of('.');
  std::string first_part_of_name;
  if (name_dot_pos == std::string::npos) {
    first_part_of_name = name;
  } else {
    first_part_of_name = name.substr(0, name_dot_pos);
  }

  std::string scope_to_try(relative_to);

  while (true) {
    // Chop off the last component of the scope.
    std::string::size_type dot_pos = scope_to_try.find_last_of('.');
    if (dot_pos == std::string::npos) {
      return FindSymbol(name, build_it);
    } else {
      scope_to_try.erase(dot_pos);
    }

    // Append ".first_part_of_name" and try to find.
    std::string::size_type old_size = scope_to_try.size();
    scope_to_try.append(1, '.');
    scope_to_try.append(first_part_of_name);
    Symbol result = FindSymbol(scope_to_try, build_it);
    if (!result.IsNull()) {
      if (first_part_of_name.size() < name.size()) {
        // name is a compound symbol, of which we only found the first part.
        // Now try to look up the rest of it.
        if (result.IsAggregate()) {
          scope_to_try.append(name, first_part_of_name.size(),
                              name.size() - first_part_of_name.size());
          result = FindSymbol(scope_to_try, build_it);
          if (result.IsNull()) {
            undefine_resolved_name_ = scope_to_try;
          }
          return result;
        } else {
          // We found a symbol but it's not an aggregate.  Continue the loop.
        }
      } else {
        if (resolve_mode == LOOKUP_TYPES && !result.IsType()) {
          // We found a symbol but it's not a type.  Continue the loop.
        } else {
          return result;
        }
      }
    }

    // Not found.  Remove the name so we can try again.
    scope_to_try.erase(old_size);
  }
}

Symbol DescriptorBuilder::LookupSymbol(
    const std::string& name, const std::string& relative_to,
    DescriptorPool::PlaceholderType placeholder_type, ResolveMode resolve_mode,
    bool build_it) {
  Symbol result =
      LookupSymbolNoPlaceholder(name, relative_to, resolve_mode, build_it);
  if (result.IsNull() && pool_->allow_unknown_) {
    // Not found, but AllowUnknownDependencies() is enabled.  Return a
    // placeholder instead.
    result = pool_->NewPlaceholderWithMutexHeld(name, placeholder_type);
  }
  return result;
}

static bool ValidateQualifiedName(const std::string& name) {
  bool last_was_period = false;

  for (int i = 0; i < name.size(); i++) {
    // I don't trust isalnum() due to locales.  :(
    if (('a' <= name[i] && name[i] <= 'z') ||
        ('A' <= name[i] && name[i] <= 'Z') ||
        ('0' <= name[i] && name[i] <= '9') || (name[i] == '_')) {
      last_was_period = false;
    } else if (name[i] == '.') {
      if (last_was_period) return false;
      last_was_period = true;
    } else {
      return false;
    }
  }

  return !name.empty() && !last_was_period;
}

Symbol DescriptorPool::NewPlaceholder(const std::string& name,
                                      PlaceholderType placeholder_type) const {
  MutexLockMaybe lock(mutex_);
  return NewPlaceholderWithMutexHeld(name, placeholder_type);
}

Symbol DescriptorPool::NewPlaceholderWithMutexHeld(
    const std::string& name, PlaceholderType placeholder_type) const {
  if (mutex_) {
    mutex_->AssertHeld();
  }
  // Compute names.
  const std::string* placeholder_full_name;
  const std::string* placeholder_name;
  const std::string* placeholder_package;

  if (!ValidateQualifiedName(name)) return kNullSymbol;
  if (name[0] == '.') {
    // Fully-qualified.
    placeholder_full_name = tables_->AllocateString(name.substr(1));
  } else {
    placeholder_full_name = tables_->AllocateString(name);
  }

  std::string::size_type dotpos = placeholder_full_name->find_last_of('.');
  if (dotpos != std::string::npos) {
    placeholder_package =
        tables_->AllocateString(placeholder_full_name->substr(0, dotpos));
    placeholder_name =
        tables_->AllocateString(placeholder_full_name->substr(dotpos + 1));
  } else {
    placeholder_package = &internal::GetEmptyString();
    placeholder_name = placeholder_full_name;
  }

  // Create the placeholders.
  FileDescriptor* placeholder_file = NewPlaceholderFileWithMutexHeld(
      *placeholder_full_name + ".placeholder.proto");
  placeholder_file->package_ = placeholder_package;

  if (placeholder_type == PLACEHOLDER_ENUM) {
    placeholder_file->enum_type_count_ = 1;
    placeholder_file->enum_types_ = tables_->AllocateArray<EnumDescriptor>(1);

    EnumDescriptor* placeholder_enum = &placeholder_file->enum_types_[0];
    memset(static_cast<void*>(placeholder_enum), 0, sizeof(*placeholder_enum));

    placeholder_enum->full_name_ = placeholder_full_name;
    placeholder_enum->name_ = placeholder_name;
    placeholder_enum->file_ = placeholder_file;
    placeholder_enum->options_ = &EnumOptions::default_instance();
    placeholder_enum->is_placeholder_ = true;
    placeholder_enum->is_unqualified_placeholder_ = (name[0] != '.');

    // Enums must have at least one value.
    placeholder_enum->value_count_ = 1;
    placeholder_enum->values_ = tables_->AllocateArray<EnumValueDescriptor>(1);

    EnumValueDescriptor* placeholder_value = &placeholder_enum->values_[0];
    memset(static_cast<void*>(placeholder_value), 0,
           sizeof(*placeholder_value));

    placeholder_value->name_ = tables_->AllocateString("PLACEHOLDER_VALUE");
    // Note that enum value names are siblings of their type, not children.
    placeholder_value->full_name_ =
        placeholder_package->empty()
            ? placeholder_value->name_
            : tables_->AllocateString(*placeholder_package +
                                      ".PLACEHOLDER_VALUE");

    placeholder_value->number_ = 0;
    placeholder_value->type_ = placeholder_enum;
    placeholder_value->options_ = &EnumValueOptions::default_instance();

    return Symbol(placeholder_enum);
  } else {
    placeholder_file->message_type_count_ = 1;
    placeholder_file->message_types_ = tables_->AllocateArray<Descriptor>(1);

    Descriptor* placeholder_message = &placeholder_file->message_types_[0];
    memset(static_cast<void*>(placeholder_message), 0,
           sizeof(*placeholder_message));

    placeholder_message->full_name_ = placeholder_full_name;
    placeholder_message->name_ = placeholder_name;
    placeholder_message->file_ = placeholder_file;
    placeholder_message->options_ = &MessageOptions::default_instance();
    placeholder_message->is_placeholder_ = true;
    placeholder_message->is_unqualified_placeholder_ = (name[0] != '.');

    if (placeholder_type == PLACEHOLDER_EXTENDABLE_MESSAGE) {
      placeholder_message->extension_range_count_ = 1;
      placeholder_message->extension_ranges_ =
          tables_->AllocateArray<Descriptor::ExtensionRange>(1);
      placeholder_message->extension_ranges_->start = 1;
      // kMaxNumber + 1 because ExtensionRange::end is exclusive.
      placeholder_message->extension_ranges_->end =
          FieldDescriptor::kMaxNumber + 1;
    }

    return Symbol(placeholder_message);
  }
}

FileDescriptor* DescriptorPool::NewPlaceholderFile(
    const std::string& name) const {
  MutexLockMaybe lock(mutex_);
  return NewPlaceholderFileWithMutexHeld(name);
}

FileDescriptor* DescriptorPool::NewPlaceholderFileWithMutexHeld(
    const std::string& name) const {
  if (mutex_) {
    mutex_->AssertHeld();
  }
  FileDescriptor* placeholder = tables_->Allocate<FileDescriptor>();
  memset(static_cast<void*>(placeholder), 0, sizeof(*placeholder));

  placeholder->name_ = tables_->AllocateString(name);
  placeholder->package_ = &internal::GetEmptyString();
  placeholder->pool_ = this;
  placeholder->options_ = &FileOptions::default_instance();
  placeholder->tables_ = &FileDescriptorTables::GetEmptyInstance();
  placeholder->source_code_info_ = &SourceCodeInfo::default_instance();
  placeholder->is_placeholder_ = true;
  placeholder->syntax_ = FileDescriptor::SYNTAX_PROTO2;
  placeholder->finished_building_ = true;
  // All other fields are zero or nullptr.

  return placeholder;
}

bool DescriptorBuilder::AddSymbol(const std::string& full_name,
                                  const void* parent, const std::string& name,
                                  const Message& proto, Symbol symbol) {
  // If the caller passed nullptr for the parent, the symbol is at file scope.
  // Use its file as the parent instead.
  if (parent == nullptr) parent = file_;

  if (tables_->AddSymbol(full_name, symbol)) {
    if (!file_tables_->AddAliasUnderParent(parent, name, symbol)) {
      // This is only possible if there was already an error adding something of
      // the same name.
      if (!had_errors_) {
        GOOGLE_LOG(DFATAL) << "\"" << full_name
                    << "\" not previously defined in "
                       "symbols_by_name_, but was defined in "
                       "symbols_by_parent_; this shouldn't be possible.";
      }
      return false;
    }
    return true;
  } else {
    const FileDescriptor* other_file = tables_->FindSymbol(full_name).GetFile();
    if (other_file == file_) {
      std::string::size_type dot_pos = full_name.find_last_of('.');
      if (dot_pos == std::string::npos) {
        AddError(full_name, proto, DescriptorPool::ErrorCollector::NAME,
                 "\"" + full_name + "\" is already defined.");
      } else {
        AddError(full_name, proto, DescriptorPool::ErrorCollector::NAME,
                 "\"" + full_name.substr(dot_pos + 1) +
                     "\" is already defined in \"" +
                     full_name.substr(0, dot_pos) + "\".");
      }
    } else {
      // Symbol seems to have been defined in a different file.
      AddError(full_name, proto, DescriptorPool::ErrorCollector::NAME,
               "\"" + full_name + "\" is already defined in file \"" +
                   other_file->name() + "\".");
    }
    return false;
  }
}

void DescriptorBuilder::AddPackage(const std::string& name,
                                   const Message& proto,
                                   const FileDescriptor* file) {
  if (tables_->AddSymbol(name, Symbol(file))) {
    // Success.  Also add parent package, if any.
    std::string::size_type dot_pos = name.find_last_of('.');
    if (dot_pos == std::string::npos) {
      // No parents.
      ValidateSymbolName(name, name, proto);
    } else {
      // Has parent.
      std::string* parent_name =
          tables_->AllocateString(name.substr(0, dot_pos));
      AddPackage(*parent_name, proto, file);
      ValidateSymbolName(name.substr(dot_pos + 1), name, proto);
    }
  } else {
    Symbol existing_symbol = tables_->FindSymbol(name);
    // It's OK to redefine a package.
    if (existing_symbol.type != Symbol::PACKAGE) {
      // Symbol seems to have been defined in a different file.
      AddError(name, proto, DescriptorPool::ErrorCollector::NAME,
               "\"" + name +
                   "\" is already defined (as something other than "
                   "a package) in file \"" +
                   existing_symbol.GetFile()->name() + "\".");
    }
  }
}

void DescriptorBuilder::ValidateSymbolName(const std::string& name,
                                           const std::string& full_name,
                                           const Message& proto) {
  if (name.empty()) {
    AddError(full_name, proto, DescriptorPool::ErrorCollector::NAME,
             "Missing name.");
  } else {
    for (int i = 0; i < name.size(); i++) {
      // I don't trust isalnum() due to locales.  :(
      if ((name[i] < 'a' || 'z' < name[i]) &&
          (name[i] < 'A' || 'Z' < name[i]) &&
          (name[i] < '0' || '9' < name[i]) && (name[i] != '_')) {
        AddError(full_name, proto, DescriptorPool::ErrorCollector::NAME,
                 "\"" + name + "\" is not a valid identifier.");
      }
    }
  }
}

// -------------------------------------------------------------------

// This generic implementation is good for all descriptors except
// FileDescriptor.
template <class DescriptorT>
void DescriptorBuilder::AllocateOptions(
    const typename DescriptorT::OptionsType& orig_options,
    DescriptorT* descriptor, int options_field_tag) {
  std::vector<int> options_path;
  descriptor->GetLocationPath(&options_path);
  options_path.push_back(options_field_tag);
  AllocateOptionsImpl(descriptor->full_name(), descriptor->full_name(),
                      orig_options, descriptor, options_path);
}

// We specialize for FileDescriptor.
void DescriptorBuilder::AllocateOptions(const FileOptions& orig_options,
                                        FileDescriptor* descriptor) {
  std::vector<int> options_path;
  options_path.push_back(FileDescriptorProto::kOptionsFieldNumber);
  // We add the dummy token so that LookupSymbol does the right thing.
  AllocateOptionsImpl(descriptor->package() + ".dummy", descriptor->name(),
                      orig_options, descriptor, options_path);
}

template <class DescriptorT>
void DescriptorBuilder::AllocateOptionsImpl(
    const std::string& name_scope, const std::string& element_name,
    const typename DescriptorT::OptionsType& orig_options,
    DescriptorT* descriptor, const std::vector<int>& options_path) {
  // We need to use a dummy pointer to work around a bug in older versions of
  // GCC.  Otherwise, the following two lines could be replaced with:
  //   typename DescriptorT::OptionsType* options =
  //       tables_->AllocateMessage<typename DescriptorT::OptionsType>();
  typename DescriptorT::OptionsType* const dummy = nullptr;
  typename DescriptorT::OptionsType* options = tables_->AllocateMessage(dummy);

  if (!orig_options.IsInitialized()) {
    AddError(name_scope + "." + element_name, orig_options,
             DescriptorPool::ErrorCollector::OPTION_NAME,
             "Uninterpreted option is missing name or value.");
    return;
  }

  // Avoid using MergeFrom()/CopyFrom() in this class to make it -fno-rtti
  // friendly. Without RTTI, MergeFrom() and CopyFrom() will fallback to the
  // reflection based method, which requires the Descriptor. However, we are in
  // the middle of building the descriptors, thus the deadlock.
  options->ParseFromString(orig_options.SerializeAsString());
  descriptor->options_ = options;

  // Don't add to options_to_interpret_ unless there were uninterpreted
  // options.  This not only avoids unnecessary work, but prevents a
  // bootstrapping problem when building descriptors for descriptor.proto.
  // descriptor.proto does not contain any uninterpreted options, but
  // attempting to interpret options anyway will cause
  // OptionsType::GetDescriptor() to be called which may then deadlock since
  // we're still trying to build it.
  if (options->uninterpreted_option_size() > 0) {
    options_to_interpret_.push_back(OptionsToInterpret(
        name_scope, element_name, options_path, &orig_options, options));
  }
}

// A common pattern:  We want to convert a repeated field in the descriptor
// to an array of values, calling some method to build each value.
#define BUILD_ARRAY(INPUT, OUTPUT, NAME, METHOD, PARENT) \
  OUTPUT->NAME##_count_ = INPUT.NAME##_size();           \
  AllocateArray(INPUT.NAME##_size(), &OUTPUT->NAME##s_); \
  for (int i = 0; i < INPUT.NAME##_size(); i++) {        \
    METHOD(INPUT.NAME(i), PARENT, OUTPUT->NAME##s_ + i); \
  }

void DescriptorBuilder::AddRecursiveImportError(
    const FileDescriptorProto& proto, int from_here) {
  std::string error_message("File recursively imports itself: ");
  for (int i = from_here; i < tables_->pending_files_.size(); i++) {
    error_message.append(tables_->pending_files_[i]);
    error_message.append(" -> ");
  }
  error_message.append(proto.name());

  if (from_here < tables_->pending_files_.size() - 1) {
    AddError(tables_->pending_files_[from_here + 1], proto,
             DescriptorPool::ErrorCollector::IMPORT, error_message);
  } else {
    AddError(proto.name(), proto, DescriptorPool::ErrorCollector::IMPORT,
             error_message);
  }
}

void DescriptorBuilder::AddTwiceListedError(const FileDescriptorProto& proto,
                                            int index) {
  AddError(proto.dependency(index), proto,
           DescriptorPool::ErrorCollector::IMPORT,
           "Import \"" + proto.dependency(index) + "\" was listed twice.");
}

void DescriptorBuilder::AddImportError(const FileDescriptorProto& proto,
                                       int index) {
  std::string message;
  if (pool_->fallback_database_ == nullptr) {
    message = "Import \"" + proto.dependency(index) + "\" has not been loaded.";
  } else {
    message = "Import \"" + proto.dependency(index) +
              "\" was not found or had errors.";
  }
  AddError(proto.dependency(index), proto,
           DescriptorPool::ErrorCollector::IMPORT, message);
}

static bool ExistingFileMatchesProto(const FileDescriptor* existing_file,
                                     const FileDescriptorProto& proto) {
  FileDescriptorProto existing_proto;
  existing_file->CopyTo(&existing_proto);
  // TODO(liujisi): Remove it when CopyTo supports copying syntax params when
  // syntax="proto2".
  if (existing_file->syntax() == FileDescriptor::SYNTAX_PROTO2 &&
      proto.has_syntax()) {
    existing_proto.set_syntax(
        existing_file->SyntaxName(existing_file->syntax()));
  }

  return existing_proto.SerializeAsString() == proto.SerializeAsString();
}

const FileDescriptor* DescriptorBuilder::BuildFile(
    const FileDescriptorProto& proto) {
  filename_ = proto.name();

  // Check if the file already exists and is identical to the one being built.
  // Note:  This only works if the input is canonical -- that is, it
  //   fully-qualifies all type names, has no UninterpretedOptions, etc.
  //   This is fine, because this idempotency "feature" really only exists to
  //   accommodate one hack in the proto1->proto2 migration layer.
  const FileDescriptor* existing_file = tables_->FindFile(filename_);
  if (existing_file != nullptr) {
    // File already in pool.  Compare the existing one to the input.
    if (ExistingFileMatchesProto(existing_file, proto)) {
      // They're identical.  Return the existing descriptor.
      return existing_file;
    }

    // Not a match.  The error will be detected and handled later.
  }

  // Check to see if this file is already on the pending files list.
  // TODO(kenton):  Allow recursive imports?  It may not work with some
  //   (most?) programming languages.  E.g., in C++, a forward declaration
  //   of a type is not sufficient to allow it to be used even in a
  //   generated header file due to inlining.  This could perhaps be
  //   worked around using tricks involving inserting #include statements
  //   mid-file, but that's pretty ugly, and I'm pretty sure there are
  //   some languages out there that do not allow recursive dependencies
  //   at all.
  for (int i = 0; i < tables_->pending_files_.size(); i++) {
    if (tables_->pending_files_[i] == proto.name()) {
      AddRecursiveImportError(proto, i);
      return nullptr;
    }
  }

  // If we have a fallback_database_, and we aren't doing lazy import building,
  // attempt to load all dependencies now, before checkpointing tables_.  This
  // avoids confusion with recursive checkpoints.
  if (!pool_->lazily_build_dependencies_) {
    if (pool_->fallback_database_ != nullptr) {
      tables_->pending_files_.push_back(proto.name());
      for (int i = 0; i < proto.dependency_size(); i++) {
        if (tables_->FindFile(proto.dependency(i)) == nullptr &&
            (pool_->underlay_ == nullptr ||
             pool_->underlay_->FindFileByName(proto.dependency(i)) ==
                 nullptr)) {
          // We don't care what this returns since we'll find out below anyway.
          pool_->TryFindFileInFallbackDatabase(proto.dependency(i));
        }
      }
      tables_->pending_files_.pop_back();
    }
  }

  // Checkpoint the tables so that we can roll back if something goes wrong.
  tables_->AddCheckpoint();

  FileDescriptor* result = BuildFileImpl(proto);

  file_tables_->FinalizeTables();
  if (result) {
    tables_->ClearLastCheckpoint();
    result->finished_building_ = true;
  } else {
    tables_->RollbackToLastCheckpoint();
  }

  return result;
}

FileDescriptor* DescriptorBuilder::BuildFileImpl(
    const FileDescriptorProto& proto) {
  FileDescriptor* result = tables_->Allocate<FileDescriptor>();
  file_ = result;

  result->is_placeholder_ = false;
  result->finished_building_ = false;
  SourceCodeInfo* info = nullptr;
  if (proto.has_source_code_info()) {
    info = tables_->AllocateMessage<SourceCodeInfo>();
    info->CopyFrom(proto.source_code_info());
    result->source_code_info_ = info;
  } else {
    result->source_code_info_ = &SourceCodeInfo::default_instance();
  }

  file_tables_ = tables_->AllocateFileTables();
  file_->tables_ = file_tables_;

  if (!proto.has_name()) {
    AddError("", proto, DescriptorPool::ErrorCollector::OTHER,
             "Missing field: FileDescriptorProto.name.");
  }

  // TODO(liujisi): Report error when the syntax is empty after all the protos
  // have added the syntax statement.
  if (proto.syntax().empty() || proto.syntax() == "proto2") {
    file_->syntax_ = FileDescriptor::SYNTAX_PROTO2;
  } else if (proto.syntax() == "proto3") {
    file_->syntax_ = FileDescriptor::SYNTAX_PROTO3;
  } else {
    file_->syntax_ = FileDescriptor::SYNTAX_UNKNOWN;
    AddError(proto.name(), proto, DescriptorPool::ErrorCollector::OTHER,
             "Unrecognized syntax: " + proto.syntax());
  }

  result->name_ = tables_->AllocateString(proto.name());
  if (proto.has_package()) {
    result->package_ = tables_->AllocateString(proto.package());
  } else {
    // We cannot rely on proto.package() returning a valid string if
    // proto.has_package() is false, because we might be running at static
    // initialization time, in which case default values have not yet been
    // initialized.
    result->package_ = tables_->AllocateString("");
  }
  result->pool_ = pool_;

  // Add to tables.
  if (!tables_->AddFile(result)) {
    AddError(proto.name(), proto, DescriptorPool::ErrorCollector::OTHER,
             "A file with this name is already in the pool.");
    // Bail out early so that if this is actually the exact same file, we
    // don't end up reporting that every single symbol is already defined.
    return nullptr;
  }
  if (!result->package().empty()) {
    AddPackage(result->package(), proto, result);
  }

  // Make sure all dependencies are loaded.
  std::set<std::string> seen_dependencies;
  result->dependency_count_ = proto.dependency_size();
  result->dependencies_ =
      tables_->AllocateArray<const FileDescriptor*>(proto.dependency_size());
  if (pool_->lazily_build_dependencies_) {
    result->dependencies_once_ = tables_->AllocateOnceDynamic();
    result->dependencies_names_ =
        tables_->AllocateArray<const std::string*>(proto.dependency_size());
    if (proto.dependency_size() > 0) {
      memset(result->dependencies_names_, 0,
             sizeof(*result->dependencies_names_) * proto.dependency_size());
    }
  } else {
    result->dependencies_once_ = nullptr;
    result->dependencies_names_ = nullptr;
  }
  unused_dependency_.clear();
  std::set<int> weak_deps;
  for (int i = 0; i < proto.weak_dependency_size(); ++i) {
    weak_deps.insert(proto.weak_dependency(i));
  }
  for (int i = 0; i < proto.dependency_size(); i++) {
    if (!seen_dependencies.insert(proto.dependency(i)).second) {
      AddTwiceListedError(proto, i);
    }

    const FileDescriptor* dependency = tables_->FindFile(proto.dependency(i));
    if (dependency == nullptr && pool_->underlay_ != nullptr) {
      dependency = pool_->underlay_->FindFileByName(proto.dependency(i));
    }

    if (dependency == result) {
      // Recursive import.  dependency/result is not fully initialized, and it's
      // dangerous to try to do anything with it.  The recursive import error
      // will be detected and reported in DescriptorBuilder::BuildFile().
      return nullptr;
    }

    if (dependency == nullptr) {
      if (!pool_->lazily_build_dependencies_) {
        if (pool_->allow_unknown_ ||
            (!pool_->enforce_weak_ && weak_deps.find(i) != weak_deps.end())) {
          dependency =
              pool_->NewPlaceholderFileWithMutexHeld(proto.dependency(i));
        } else {
          AddImportError(proto, i);
        }
      }
    } else {
      // Add to unused_dependency_ to track unused imported files.
      // Note: do not track unused imported files for public import.
      if (pool_->enforce_dependencies_ &&
          (pool_->unused_import_track_files_.find(proto.name()) !=
           pool_->unused_import_track_files_.end()) &&
          (dependency->public_dependency_count() == 0)) {
        unused_dependency_.insert(dependency);
      }
    }

    result->dependencies_[i] = dependency;
    if (pool_->lazily_build_dependencies_ && !dependency) {
      result->dependencies_names_[i] =
          tables_->AllocateString(proto.dependency(i));
    }
  }

  // Check public dependencies.
  int public_dependency_count = 0;
  result->public_dependencies_ =
      tables_->AllocateArray<int>(proto.public_dependency_size());
  for (int i = 0; i < proto.public_dependency_size(); i++) {
    // Only put valid public dependency indexes.
    int index = proto.public_dependency(i);
    if (index >= 0 && index < proto.dependency_size()) {
      result->public_dependencies_[public_dependency_count++] = index;
      // Do not track unused imported files for public import.
      // Calling dependency(i) builds that file when doing lazy imports,
      // need to avoid doing this. Unused dependency detection isn't done
      // when building lazily, anyways.
      if (!pool_->lazily_build_dependencies_) {
        unused_dependency_.erase(result->dependency(index));
      }
    } else {
      AddError(proto.name(), proto, DescriptorPool::ErrorCollector::OTHER,
               "Invalid public dependency index.");
    }
  }
  result->public_dependency_count_ = public_dependency_count;

  // Build dependency set
  dependencies_.clear();
  // We don't/can't do proper dependency error checking when
  // lazily_build_dependencies_, and calling dependency(i) will force
  // a dependency to be built, which we don't want.
  if (!pool_->lazily_build_dependencies_) {
    for (int i = 0; i < result->dependency_count(); i++) {
      RecordPublicDependencies(result->dependency(i));
    }
  }

  // Check weak dependencies.
  int weak_dependency_count = 0;
  result->weak_dependencies_ =
      tables_->AllocateArray<int>(proto.weak_dependency_size());
  for (int i = 0; i < proto.weak_dependency_size(); i++) {
    int index = proto.weak_dependency(i);
    if (index >= 0 && index < proto.dependency_size()) {
      result->weak_dependencies_[weak_dependency_count++] = index;
    } else {
      AddError(proto.name(), proto, DescriptorPool::ErrorCollector::OTHER,
               "Invalid weak dependency index.");
    }
  }
  result->weak_dependency_count_ = weak_dependency_count;

  // Convert children.
  BUILD_ARRAY(proto, result, message_type, BuildMessage, nullptr);
  BUILD_ARRAY(proto, result, enum_type, BuildEnum, nullptr);
  BUILD_ARRAY(proto, result, service, BuildService, nullptr);
  BUILD_ARRAY(proto, result, extension, BuildExtension, nullptr);

  // Copy options.
  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    AllocateOptions(proto.options(), result);
  }

  // Note that the following steps must occur in exactly the specified order.

  // Cross-link.
  CrossLinkFile(result, proto);

  // Interpret any remaining uninterpreted options gathered into
  // options_to_interpret_ during descriptor building.  Cross-linking has made
  // extension options known, so all interpretations should now succeed.
  if (!had_errors_) {
    OptionInterpreter option_interpreter(this);
    for (std::vector<OptionsToInterpret>::iterator iter =
             options_to_interpret_.begin();
         iter != options_to_interpret_.end(); ++iter) {
      option_interpreter.InterpretOptions(&(*iter));
    }
    options_to_interpret_.clear();
    if (info != nullptr) {
      option_interpreter.UpdateSourceCodeInfo(info);
    }
  }

  // Validate options. See comments at InternalSetLazilyBuildDependencies about
  // error checking and lazy import building.
  if (!had_errors_ && !pool_->lazily_build_dependencies_) {
    ValidateFileOptions(result, proto);
  }

  // Additional naming conflict check for map entry types. Only need to check
  // this if there are already errors.
  if (had_errors_) {
    for (int i = 0; i < proto.message_type_size(); ++i) {
      DetectMapConflicts(result->message_type(i), proto.message_type(i));
    }
  }


  // Again, see comments at InternalSetLazilyBuildDependencies about error
  // checking.
  if (!unused_dependency_.empty() && !pool_->lazily_build_dependencies_) {
    LogUnusedDependency(proto, result);
  }

  if (had_errors_) {
    return nullptr;
  } else {
    return result;
  }
}


std::string* DescriptorBuilder::AllocateNameString(
    const std::string& scope, const std::string& proto_name) {
  std::string* full_name;
  if (scope.empty()) {
    full_name = tables_->AllocateString(proto_name);
  } else {
    full_name = tables_->AllocateEmptyString();
    *full_name = StrCat(scope, ".", proto_name);
  }
  return full_name;
}

void DescriptorBuilder::BuildMessage(const DescriptorProto& proto,
                                     const Descriptor* parent,
                                     Descriptor* result) {
  const std::string& scope =
      (parent == nullptr) ? file_->package() : parent->full_name();
  std::string* full_name = AllocateNameString(scope, proto.name());
  ValidateSymbolName(proto.name(), *full_name, proto);

  result->name_ = tables_->AllocateString(proto.name());
  result->full_name_ = full_name;
  result->file_ = file_;
  result->containing_type_ = parent;
  result->is_placeholder_ = false;
  result->is_unqualified_placeholder_ = false;

  // Build oneofs first so that fields and extension ranges can refer to them.
  BUILD_ARRAY(proto, result, oneof_decl, BuildOneof, result);
  BUILD_ARRAY(proto, result, field, BuildField, result);
  BUILD_ARRAY(proto, result, nested_type, BuildMessage, result);
  BUILD_ARRAY(proto, result, enum_type, BuildEnum, result);
  BUILD_ARRAY(proto, result, extension_range, BuildExtensionRange, result);
  BUILD_ARRAY(proto, result, extension, BuildExtension, result);
  BUILD_ARRAY(proto, result, reserved_range, BuildReservedRange, result);

  // Copy reserved names.
  int reserved_name_count = proto.reserved_name_size();
  result->reserved_name_count_ = reserved_name_count;
  result->reserved_names_ =
      tables_->AllocateArray<const std::string*>(reserved_name_count);
  for (int i = 0; i < reserved_name_count; ++i) {
    result->reserved_names_[i] =
        tables_->AllocateString(proto.reserved_name(i));
  }

  // Copy options.
  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    AllocateOptions(proto.options(), result,
                    DescriptorProto::kOptionsFieldNumber);
  }

  AddSymbol(result->full_name(), parent, result->name(), proto, Symbol(result));


  for (int i = 0; i < proto.reserved_range_size(); i++) {
    const DescriptorProto_ReservedRange& range1 = proto.reserved_range(i);
    for (int j = i + 1; j < proto.reserved_range_size(); j++) {
      const DescriptorProto_ReservedRange& range2 = proto.reserved_range(j);
      if (range1.end() > range2.start() && range2.end() > range1.start()) {
        AddError(result->full_name(), proto.reserved_range(i),
                 DescriptorPool::ErrorCollector::NUMBER,
                 strings::Substitute("Reserved range $0 to $1 overlaps with "
                                     "already-defined range $2 to $3.",
                                     range2.start(), range2.end() - 1,
                                     range1.start(), range1.end() - 1));
      }
    }
  }

  HASH_SET<std::string> reserved_name_set;
  for (int i = 0; i < proto.reserved_name_size(); i++) {
    const std::string& name = proto.reserved_name(i);
    if (reserved_name_set.find(name) == reserved_name_set.end()) {
      reserved_name_set.insert(name);
    } else {
      AddError(name, proto, DescriptorPool::ErrorCollector::NAME,
               strings::Substitute(
                   "Field name \"$0\" is reserved multiple times.", name));
    }
  }

  for (int i = 0; i < result->field_count(); i++) {
    const FieldDescriptor* field = result->field(i);
    for (int j = 0; j < result->extension_range_count(); j++) {
      const Descriptor::ExtensionRange* range = result->extension_range(j);
      if (range->start <= field->number() && field->number() < range->end) {
        AddError(
            field->full_name(), proto.extension_range(j),
            DescriptorPool::ErrorCollector::NUMBER,
            strings::Substitute(
                "Extension range $0 to $1 includes field \"$2\" ($3).",
                range->start, range->end - 1, field->name(), field->number()));
      }
    }
    for (int j = 0; j < result->reserved_range_count(); j++) {
      const Descriptor::ReservedRange* range = result->reserved_range(j);
      if (range->start <= field->number() && field->number() < range->end) {
        AddError(field->full_name(), proto.reserved_range(j),
                 DescriptorPool::ErrorCollector::NUMBER,
                 strings::Substitute("Field \"$0\" uses reserved number $1.",
                                     field->name(), field->number()));
      }
    }
    if (reserved_name_set.find(field->name()) != reserved_name_set.end()) {
      AddError(
          field->full_name(), proto.field(i),
          DescriptorPool::ErrorCollector::NAME,
          strings::Substitute("Field name \"$0\" is reserved.", field->name()));
    }
  }

  // Check that extension ranges don't overlap and don't include
  // reserved field numbers.
  for (int i = 0; i < result->extension_range_count(); i++) {
    const Descriptor::ExtensionRange* range1 = result->extension_range(i);
    for (int j = 0; j < result->reserved_range_count(); j++) {
      const Descriptor::ReservedRange* range2 = result->reserved_range(j);
      if (range1->end > range2->start && range2->end > range1->start) {
        AddError(result->full_name(), proto.extension_range(i),
                 DescriptorPool::ErrorCollector::NUMBER,
                 strings::Substitute("Extension range $0 to $1 overlaps with "
                                     "reserved range $2 to $3.",
                                     range1->start, range1->end - 1,
                                     range2->start, range2->end - 1));
      }
    }
    for (int j = i + 1; j < result->extension_range_count(); j++) {
      const Descriptor::ExtensionRange* range2 = result->extension_range(j);
      if (range1->end > range2->start && range2->end > range1->start) {
        AddError(result->full_name(), proto.extension_range(i),
                 DescriptorPool::ErrorCollector::NUMBER,
                 strings::Substitute("Extension range $0 to $1 overlaps with "
                                     "already-defined range $2 to $3.",
                                     range2->start, range2->end - 1,
                                     range1->start, range1->end - 1));
      }
    }
  }
}

void DescriptorBuilder::BuildFieldOrExtension(const FieldDescriptorProto& proto,
                                              const Descriptor* parent,
                                              FieldDescriptor* result,
                                              bool is_extension) {
  const std::string& scope =
      (parent == nullptr) ? file_->package() : parent->full_name();
  std::string* full_name = AllocateNameString(scope, proto.name());
  ValidateSymbolName(proto.name(), *full_name, proto);

  result->name_ = tables_->AllocateString(proto.name());
  result->full_name_ = full_name;
  result->file_ = file_;
  result->number_ = proto.number();
  result->is_extension_ = is_extension;

  // If .proto files follow the style guide then the name should already be
  // lower-cased.  If that's the case we can just reuse the string we
  // already allocated rather than allocate a new one.
  std::string lowercase_name(proto.name());
  LowerString(&lowercase_name);
  if (lowercase_name == proto.name()) {
    result->lowercase_name_ = result->name_;
  } else {
    result->lowercase_name_ = tables_->AllocateString(lowercase_name);
  }

  // Don't bother with the above optimization for camel-case names since
  // .proto files that follow the guide shouldn't be using names in this
  // format, so the optimization wouldn't help much.
  result->camelcase_name_ =
      tables_->AllocateString(ToCamelCase(proto.name(),
                                          /* lower_first = */ true));

  if (proto.has_json_name()) {
    result->has_json_name_ = true;
    result->json_name_ = tables_->AllocateString(proto.json_name());
  } else {
    result->has_json_name_ = false;
    result->json_name_ = tables_->AllocateString(ToJsonName(proto.name()));
  }

  // Some compilers do not allow static_cast directly between two enum types,
  // so we must cast to int first.
  result->type_ = static_cast<FieldDescriptor::Type>(
      implicit_cast<int>(proto.type()));
  result->label_ = static_cast<FieldDescriptor::Label>(
      implicit_cast<int>(proto.label()));

  // An extension cannot have a required field (b/13365836).
  if (result->is_extension_ &&
      result->label_ == FieldDescriptor::LABEL_REQUIRED) {
    AddError(result->full_name(), proto,
             // Error location `TYPE`: we would really like to indicate
             // `LABEL`, but the `ErrorLocation` enum has no entry for this, and
             // we don't necessarily know about all implementations of the
             // `ErrorCollector` interface to extend them to handle the new
             // error location type properly.
             DescriptorPool::ErrorCollector::TYPE,
             "Message extensions cannot have required fields.");
  }

  // Some of these may be filled in when cross-linking.
  result->containing_type_ = nullptr;
  result->extension_scope_ = nullptr;
  result->message_type_ = nullptr;
  result->enum_type_ = nullptr;
  result->type_name_ = nullptr;
  result->type_once_ = nullptr;
  result->default_value_enum_ = nullptr;
  result->default_value_enum_name_ = nullptr;

  result->has_default_value_ = proto.has_default_value();
  if (proto.has_default_value() && result->is_repeated()) {
    AddError(result->full_name(), proto,
             DescriptorPool::ErrorCollector::DEFAULT_VALUE,
             "Repeated fields can't have default values.");
  }

  if (proto.has_type()) {
    if (proto.has_default_value()) {
      char* end_pos = nullptr;
      switch (result->cpp_type()) {
        case FieldDescriptor::CPPTYPE_INT32:
          result->default_value_int32_ =
              strtol(proto.default_value().c_str(), &end_pos, 0);
          break;
        case FieldDescriptor::CPPTYPE_INT64:
          result->default_value_int64_ =
              strto64(proto.default_value().c_str(), &end_pos, 0);
          break;
        case FieldDescriptor::CPPTYPE_UINT32:
          result->default_value_uint32_ =
              strtoul(proto.default_value().c_str(), &end_pos, 0);
          break;
        case FieldDescriptor::CPPTYPE_UINT64:
          result->default_value_uint64_ =
              strtou64(proto.default_value().c_str(), &end_pos, 0);
          break;
        case FieldDescriptor::CPPTYPE_FLOAT:
          if (proto.default_value() == "inf") {
            result->default_value_float_ =
                std::numeric_limits<float>::infinity();
          } else if (proto.default_value() == "-inf") {
            result->default_value_float_ =
                -std::numeric_limits<float>::infinity();
          } else if (proto.default_value() == "nan") {
            result->default_value_float_ =
                std::numeric_limits<float>::quiet_NaN();
          } else {
            result->default_value_float_ = io::SafeDoubleToFloat(
                io::NoLocaleStrtod(proto.default_value().c_str(), &end_pos));
          }
          break;
        case FieldDescriptor::CPPTYPE_DOUBLE:
          if (proto.default_value() == "inf") {
            result->default_value_double_ =
                std::numeric_limits<double>::infinity();
          } else if (proto.default_value() == "-inf") {
            result->default_value_double_ =
                -std::numeric_limits<double>::infinity();
          } else if (proto.default_value() == "nan") {
            result->default_value_double_ =
                std::numeric_limits<double>::quiet_NaN();
          } else {
            result->default_value_double_ =
                io::NoLocaleStrtod(proto.default_value().c_str(), &end_pos);
          }
          break;
        case FieldDescriptor::CPPTYPE_BOOL:
          if (proto.default_value() == "true") {
            result->default_value_bool_ = true;
          } else if (proto.default_value() == "false") {
            result->default_value_bool_ = false;
          } else {
            AddError(result->full_name(), proto,
                     DescriptorPool::ErrorCollector::DEFAULT_VALUE,
                     "Boolean default must be true or false.");
          }
          break;
        case FieldDescriptor::CPPTYPE_ENUM:
          // This will be filled in when cross-linking.
          result->default_value_enum_ = nullptr;
          break;
        case FieldDescriptor::CPPTYPE_STRING:
          if (result->type() == FieldDescriptor::TYPE_BYTES) {
            result->default_value_string_ = tables_->AllocateString(
                UnescapeCEscapeString(proto.default_value()));
          } else {
            result->default_value_string_ =
                tables_->AllocateString(proto.default_value());
          }
          break;
        case FieldDescriptor::CPPTYPE_MESSAGE:
          AddError(result->full_name(), proto,
                   DescriptorPool::ErrorCollector::DEFAULT_VALUE,
                   "Messages can't have default values.");
          result->has_default_value_ = false;
          break;
      }

      if (end_pos != nullptr) {
        // end_pos is only set non-null by the parsers for numeric types,
        // above. This checks that the default was non-empty and had no extra
        // junk after the end of the number.
        if (proto.default_value().empty() || *end_pos != '\0') {
          AddError(result->full_name(), proto,
                   DescriptorPool::ErrorCollector::DEFAULT_VALUE,
                   "Couldn't parse default value \"" + proto.default_value() +
                       "\".");
        }
      }
    } else {
      // No explicit default value
      switch (result->cpp_type()) {
        case FieldDescriptor::CPPTYPE_INT32:
          result->default_value_int32_ = 0;
          break;
        case FieldDescriptor::CPPTYPE_INT64:
          result->default_value_int64_ = 0;
          break;
        case FieldDescriptor::CPPTYPE_UINT32:
          result->default_value_uint32_ = 0;
          break;
        case FieldDescriptor::CPPTYPE_UINT64:
          result->default_value_uint64_ = 0;
          break;
        case FieldDescriptor::CPPTYPE_FLOAT:
          result->default_value_float_ = 0.0f;
          break;
        case FieldDescriptor::CPPTYPE_DOUBLE:
          result->default_value_double_ = 0.0;
          break;
        case FieldDescriptor::CPPTYPE_BOOL:
          result->default_value_bool_ = false;
          break;
        case FieldDescriptor::CPPTYPE_ENUM:
          // This will be filled in when cross-linking.
          result->default_value_enum_ = nullptr;
          break;
        case FieldDescriptor::CPPTYPE_STRING:
          result->default_value_string_ = &internal::GetEmptyString();
          break;
        case FieldDescriptor::CPPTYPE_MESSAGE:
          break;
      }
    }
  }

  if (result->number() <= 0) {
    AddError(result->full_name(), proto, DescriptorPool::ErrorCollector::NUMBER,
             "Field numbers must be positive integers.");
  } else if (!is_extension && result->number() > FieldDescriptor::kMaxNumber) {
    // Only validate that the number is within the valid field range if it is
    // not an extension. Since extension numbers are validated with the
    // extendee's valid set of extension numbers, and those are in turn
    // validated against the max allowed number, the check is unnecessary for
    // extension fields.
    // This avoids cross-linking issues that arise when attempting to check if
    // the extendee is a message_set_wire_format message, which has a higher max
    // on extension numbers.
    AddError(result->full_name(), proto, DescriptorPool::ErrorCollector::NUMBER,
             strings::Substitute("Field numbers cannot be greater than $0.",
                                 FieldDescriptor::kMaxNumber));
  } else if (result->number() >= FieldDescriptor::kFirstReservedNumber &&
             result->number() <= FieldDescriptor::kLastReservedNumber) {
    AddError(result->full_name(), proto, DescriptorPool::ErrorCollector::NUMBER,
             strings::Substitute(
                 "Field numbers $0 through $1 are reserved for the protocol "
                 "buffer library implementation.",
                 FieldDescriptor::kFirstReservedNumber,
                 FieldDescriptor::kLastReservedNumber));
  }

  if (is_extension) {
    if (!proto.has_extendee()) {
      AddError(result->full_name(), proto,
               DescriptorPool::ErrorCollector::EXTENDEE,
               "FieldDescriptorProto.extendee not set for extension field.");
    }

    result->extension_scope_ = parent;

    if (proto.has_oneof_index()) {
      AddError(result->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "FieldDescriptorProto.oneof_index should not be set for "
               "extensions.");
    }

    // Fill in later (maybe).
    result->containing_oneof_ = nullptr;
  } else {
    if (proto.has_extendee()) {
      AddError(result->full_name(), proto,
               DescriptorPool::ErrorCollector::EXTENDEE,
               "FieldDescriptorProto.extendee set for non-extension field.");
    }

    result->containing_type_ = parent;

    if (proto.has_oneof_index()) {
      if (proto.oneof_index() < 0 ||
          proto.oneof_index() >= parent->oneof_decl_count()) {
        AddError(result->full_name(), proto,
                 DescriptorPool::ErrorCollector::TYPE,
                 strings::Substitute("FieldDescriptorProto.oneof_index $0 is "
                                     "out of range for type \"$1\".",
                                     proto.oneof_index(), parent->name()));
        result->containing_oneof_ = nullptr;
      } else {
        result->containing_oneof_ = parent->oneof_decl(proto.oneof_index());
      }
    } else {
      result->containing_oneof_ = nullptr;
    }
  }

  // Copy options.
  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    AllocateOptions(proto.options(), result,
                    FieldDescriptorProto::kOptionsFieldNumber);
  }


  AddSymbol(result->full_name(), parent, result->name(), proto, Symbol(result));
}

void DescriptorBuilder::BuildExtensionRange(
    const DescriptorProto::ExtensionRange& proto, const Descriptor* parent,
    Descriptor::ExtensionRange* result) {
  result->start = proto.start();
  result->end = proto.end();
  if (result->start <= 0) {
    AddError(parent->full_name(), proto, DescriptorPool::ErrorCollector::NUMBER,
             "Extension numbers must be positive integers.");
  }

  // Checking of the upper bound of the extension range is deferred until after
  // options interpreting. This allows messages with message_set_wire_format to
  // have extensions beyond FieldDescriptor::kMaxNumber, since the extension
  // numbers are actually used as int32s in the message_set_wire_format.

  if (result->start >= result->end) {
    AddError(parent->full_name(), proto, DescriptorPool::ErrorCollector::NUMBER,
             "Extension range end number must be greater than start number.");
  }

  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    std::vector<int> options_path;
    parent->GetLocationPath(&options_path);
    options_path.push_back(DescriptorProto::kExtensionRangeFieldNumber);
    // find index of this extension range in order to compute path
    int index;
    for (index = 0; parent->extension_ranges_ + index != result; index++) {
    }
    options_path.push_back(index);
    options_path.push_back(DescriptorProto_ExtensionRange::kOptionsFieldNumber);
    AllocateOptionsImpl(parent->full_name(), parent->full_name(),
                        proto.options(), result, options_path);
  }
}

void DescriptorBuilder::BuildReservedRange(
    const DescriptorProto::ReservedRange& proto, const Descriptor* parent,
    Descriptor::ReservedRange* result) {
  result->start = proto.start();
  result->end = proto.end();
  if (result->start <= 0) {
    AddError(parent->full_name(), proto, DescriptorPool::ErrorCollector::NUMBER,
             "Reserved numbers must be positive integers.");
  }
}

void DescriptorBuilder::BuildReservedRange(
    const EnumDescriptorProto::EnumReservedRange& proto,
    const EnumDescriptor* parent, EnumDescriptor::ReservedRange* result) {
  result->start = proto.start();
  result->end = proto.end();

  if (result->start > result->end) {
    AddError(parent->full_name(), proto, DescriptorPool::ErrorCollector::NUMBER,
             "Reserved range end number must be greater than start number.");
  }
}

void DescriptorBuilder::BuildOneof(const OneofDescriptorProto& proto,
                                   Descriptor* parent,
                                   OneofDescriptor* result) {
  std::string* full_name =
      AllocateNameString(parent->full_name(), proto.name());
  ValidateSymbolName(proto.name(), *full_name, proto);

  result->name_ = tables_->AllocateString(proto.name());
  result->full_name_ = full_name;

  result->containing_type_ = parent;

  // We need to fill these in later.
  result->field_count_ = 0;
  result->fields_ = nullptr;

  // Copy options.
  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    AllocateOptions(proto.options(), result,
                    OneofDescriptorProto::kOptionsFieldNumber);
  }

  AddSymbol(result->full_name(), parent, result->name(), proto, Symbol(result));
}

void DescriptorBuilder::CheckEnumValueUniqueness(
    const EnumDescriptorProto& proto, const EnumDescriptor* result) {

  // Check that enum labels are still unique when we remove the enum prefix from
  // values that have it.
  //
  // This will fail for something like:
  //
  //   enum MyEnum {
  //     MY_ENUM_FOO = 0;
  //     FOO = 1;
  //   }
  //
  // By enforcing this reasonable constraint, we allow code generators to strip
  // the prefix and/or PascalCase it without creating conflicts.  This can lead
  // to much nicer language-specific enums like:
  //
  //   enum NameType {
  //     FirstName = 1,
  //     LastName = 2,
  //   }
  //
  // Instead of:
  //
  //   enum NameType {
  //     NAME_TYPE_FIRST_NAME = 1,
  //     NAME_TYPE_LAST_NAME = 2,
  //   }
  PrefixRemover remover(result->name());
  std::map<std::string, const EnumValueDescriptor*> values;
  for (int i = 0; i < result->value_count(); i++) {
    const EnumValueDescriptor* value = result->value(i);
    std::string stripped =
        EnumValueToPascalCase(remover.MaybeRemove(value->name()));
    std::pair<std::map<std::string, const EnumValueDescriptor*>::iterator, bool>
        insert_result = values.insert(std::make_pair(stripped, value));
    bool inserted = insert_result.second;

    // We don't throw the error if the two conflicting symbols are identical, or
    // if they map to the same number.  In the former case, the normal symbol
    // duplication error will fire so we don't need to (and its error message
    // will make more sense). We allow the latter case so users can create
    // aliases which add or remove the prefix (code generators that do prefix
    // stripping should de-dup the labels in this case).
    if (!inserted && insert_result.first->second->name() != value->name() &&
        insert_result.first->second->number() != value->number()) {
      std::string error_message =
          "Enum name " + value->name() + " has the same name as " +
          values[stripped]->name() +
          " if you ignore case and strip out the enum name prefix (if any). "
          "This is error-prone and can lead to undefined behavior. "
          "Please avoid doing this. If you are using allow_alias, please "
          "assign the same numeric value to both enums.";
      // There are proto2 enums out there with conflicting names, so to preserve
      // compatibility we issue only a warning for proto2.
      if (result->file()->syntax() == FileDescriptor::SYNTAX_PROTO2) {
        AddWarning(value->full_name(), proto.value(i),
                   DescriptorPool::ErrorCollector::NAME, error_message);
      } else {
        AddError(value->full_name(), proto.value(i),
                 DescriptorPool::ErrorCollector::NAME, error_message);
      }
    }
  }
}

void DescriptorBuilder::BuildEnum(const EnumDescriptorProto& proto,
                                  const Descriptor* parent,
                                  EnumDescriptor* result) {
  const std::string& scope =
      (parent == nullptr) ? file_->package() : parent->full_name();
  std::string* full_name = AllocateNameString(scope, proto.name());
  ValidateSymbolName(proto.name(), *full_name, proto);

  result->name_ = tables_->AllocateString(proto.name());
  result->full_name_ = full_name;
  result->file_ = file_;
  result->containing_type_ = parent;
  result->is_placeholder_ = false;
  result->is_unqualified_placeholder_ = false;

  if (proto.value_size() == 0) {
    // We cannot allow enums with no values because this would mean there
    // would be no valid default value for fields of this type.
    AddError(result->full_name(), proto, DescriptorPool::ErrorCollector::NAME,
             "Enums must contain at least one value.");
  }

  BUILD_ARRAY(proto, result, value, BuildEnumValue, result);
  BUILD_ARRAY(proto, result, reserved_range, BuildReservedRange, result);

  // Copy reserved names.
  int reserved_name_count = proto.reserved_name_size();
  result->reserved_name_count_ = reserved_name_count;
  result->reserved_names_ =
      tables_->AllocateArray<const std::string*>(reserved_name_count);
  for (int i = 0; i < reserved_name_count; ++i) {
    result->reserved_names_[i] =
        tables_->AllocateString(proto.reserved_name(i));
  }

  CheckEnumValueUniqueness(proto, result);

  // Copy options.
  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    AllocateOptions(proto.options(), result,
                    EnumDescriptorProto::kOptionsFieldNumber);
  }

  AddSymbol(result->full_name(), parent, result->name(), proto, Symbol(result));

  for (int i = 0; i < proto.reserved_range_size(); i++) {
    const EnumDescriptorProto_EnumReservedRange& range1 =
        proto.reserved_range(i);
    for (int j = i + 1; j < proto.reserved_range_size(); j++) {
      const EnumDescriptorProto_EnumReservedRange& range2 =
          proto.reserved_range(j);
      if (range1.end() >= range2.start() && range2.end() >= range1.start()) {
        AddError(result->full_name(), proto.reserved_range(i),
                 DescriptorPool::ErrorCollector::NUMBER,
                 strings::Substitute("Reserved range $0 to $1 overlaps with "
                                     "already-defined range $2 to $3.",
                                     range2.start(), range2.end(),
                                     range1.start(), range1.end()));
      }
    }
  }

  HASH_SET<std::string> reserved_name_set;
  for (int i = 0; i < proto.reserved_name_size(); i++) {
    const std::string& name = proto.reserved_name(i);
    if (reserved_name_set.find(name) == reserved_name_set.end()) {
      reserved_name_set.insert(name);
    } else {
      AddError(name, proto, DescriptorPool::ErrorCollector::NAME,
               strings::Substitute(
                   "Enum value \"$0\" is reserved multiple times.", name));
    }
  }

  for (int i = 0; i < result->value_count(); i++) {
    const EnumValueDescriptor* value = result->value(i);
    for (int j = 0; j < result->reserved_range_count(); j++) {
      const EnumDescriptor::ReservedRange* range = result->reserved_range(j);
      if (range->start <= value->number() && value->number() <= range->end) {
        AddError(
            value->full_name(), proto.reserved_range(j),
            DescriptorPool::ErrorCollector::NUMBER,
            strings::Substitute("Enum value \"$0\" uses reserved number $1.",
                                value->name(), value->number()));
      }
    }
    if (reserved_name_set.find(value->name()) != reserved_name_set.end()) {
      AddError(
          value->full_name(), proto.value(i),
          DescriptorPool::ErrorCollector::NAME,
          strings::Substitute("Enum value \"$0\" is reserved.", value->name()));
    }
  }
}

void DescriptorBuilder::BuildEnumValue(const EnumValueDescriptorProto& proto,
                                       const EnumDescriptor* parent,
                                       EnumValueDescriptor* result) {
  result->name_ = tables_->AllocateString(proto.name());
  result->number_ = proto.number();
  result->type_ = parent;

  // Note:  full_name for enum values is a sibling to the parent's name, not a
  //   child of it.
  std::string* full_name = tables_->AllocateEmptyString();
  size_t scope_len = parent->full_name_->size() - parent->name_->size();
  full_name->reserve(scope_len + result->name_->size());
  full_name->append(parent->full_name_->data(), scope_len);
  full_name->append(*result->name_);
  result->full_name_ = full_name;

  ValidateSymbolName(proto.name(), *full_name, proto);

  // Copy options.
  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    AllocateOptions(proto.options(), result,
                    EnumValueDescriptorProto::kOptionsFieldNumber);
  }

  // Again, enum values are weird because we makes them appear as siblings
  // of the enum type instead of children of it.  So, we use
  // parent->containing_type() as the value's parent.
  bool added_to_outer_scope =
      AddSymbol(result->full_name(), parent->containing_type(), result->name(),
                proto, Symbol(result));

  // However, we also want to be able to search for values within a single
  // enum type, so we add it as a child of the enum type itself, too.
  // Note:  This could fail, but if it does, the error has already been
  //   reported by the above AddSymbol() call, so we ignore the return code.
  bool added_to_inner_scope =
      file_tables_->AddAliasUnderParent(parent, result->name(), Symbol(result));

  if (added_to_inner_scope && !added_to_outer_scope) {
    // This value did not conflict with any values defined in the same enum,
    // but it did conflict with some other symbol defined in the enum type's
    // scope.  Let's print an additional error to explain this.
    std::string outer_scope;
    if (parent->containing_type() == nullptr) {
      outer_scope = file_->package();
    } else {
      outer_scope = parent->containing_type()->full_name();
    }

    if (outer_scope.empty()) {
      outer_scope = "the global scope";
    } else {
      outer_scope = "\"" + outer_scope + "\"";
    }

    AddError(result->full_name(), proto, DescriptorPool::ErrorCollector::NAME,
             "Note that enum values use C++ scoping rules, meaning that "
             "enum values are siblings of their type, not children of it.  "
             "Therefore, \"" +
                 result->name() + "\" must be unique within " + outer_scope +
                 ", not just within \"" + parent->name() + "\".");
  }

  // An enum is allowed to define two numbers that refer to the same value.
  // FindValueByNumber() should return the first such value, so we simply
  // ignore AddEnumValueByNumber()'s return code.
  file_tables_->AddEnumValueByNumber(result);
}

void DescriptorBuilder::BuildService(const ServiceDescriptorProto& proto,
                                     const void* /* dummy */,
                                     ServiceDescriptor* result) {
  std::string* full_name = AllocateNameString(file_->package(), proto.name());
  ValidateSymbolName(proto.name(), *full_name, proto);

  result->name_ = tables_->AllocateString(proto.name());
  result->full_name_ = full_name;
  result->file_ = file_;

  BUILD_ARRAY(proto, result, method, BuildMethod, result);

  // Copy options.
  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    AllocateOptions(proto.options(), result,
                    ServiceDescriptorProto::kOptionsFieldNumber);
  }

  AddSymbol(result->full_name(), nullptr, result->name(), proto,
            Symbol(result));
}

void DescriptorBuilder::BuildMethod(const MethodDescriptorProto& proto,
                                    const ServiceDescriptor* parent,
                                    MethodDescriptor* result) {
  result->name_ = tables_->AllocateString(proto.name());
  result->service_ = parent;

  std::string* full_name =
      AllocateNameString(parent->full_name(), *result->name_);
  result->full_name_ = full_name;

  ValidateSymbolName(proto.name(), *full_name, proto);

  // These will be filled in when cross-linking.
  result->input_type_.Init();
  result->output_type_.Init();

  // Copy options.
  if (!proto.has_options()) {
    result->options_ = nullptr;  // Will set to default_instance later.
  } else {
    AllocateOptions(proto.options(), result,
                    MethodDescriptorProto::kOptionsFieldNumber);
  }

  result->client_streaming_ = proto.client_streaming();
  result->server_streaming_ = proto.server_streaming();

  AddSymbol(result->full_name(), parent, result->name(), proto, Symbol(result));
}

#undef BUILD_ARRAY

// -------------------------------------------------------------------

void DescriptorBuilder::CrossLinkFile(FileDescriptor* file,
                                      const FileDescriptorProto& proto) {
  if (file->options_ == nullptr) {
    file->options_ = &FileOptions::default_instance();
  }

  for (int i = 0; i < file->message_type_count(); i++) {
    CrossLinkMessage(&file->message_types_[i], proto.message_type(i));
  }

  for (int i = 0; i < file->extension_count(); i++) {
    CrossLinkField(&file->extensions_[i], proto.extension(i));
  }

  for (int i = 0; i < file->enum_type_count(); i++) {
    CrossLinkEnum(&file->enum_types_[i], proto.enum_type(i));
  }

  for (int i = 0; i < file->service_count(); i++) {
    CrossLinkService(&file->services_[i], proto.service(i));
  }
}

void DescriptorBuilder::CrossLinkMessage(Descriptor* message,
                                         const DescriptorProto& proto) {
  if (message->options_ == nullptr) {
    message->options_ = &MessageOptions::default_instance();
  }

  for (int i = 0; i < message->nested_type_count(); i++) {
    CrossLinkMessage(&message->nested_types_[i], proto.nested_type(i));
  }

  for (int i = 0; i < message->enum_type_count(); i++) {
    CrossLinkEnum(&message->enum_types_[i], proto.enum_type(i));
  }

  for (int i = 0; i < message->field_count(); i++) {
    CrossLinkField(&message->fields_[i], proto.field(i));
  }

  for (int i = 0; i < message->extension_count(); i++) {
    CrossLinkField(&message->extensions_[i], proto.extension(i));
  }

  for (int i = 0; i < message->extension_range_count(); i++) {
    CrossLinkExtensionRange(&message->extension_ranges_[i],
                            proto.extension_range(i));
  }

  // Set up field array for each oneof.

  // First count the number of fields per oneof.
  for (int i = 0; i < message->field_count(); i++) {
    const OneofDescriptor* oneof_decl = message->field(i)->containing_oneof();
    if (oneof_decl != nullptr) {
      // Make sure fields belonging to the same oneof are defined consecutively.
      // This enables optimizations in codegens and reflection libraries to
      // skip fields in the oneof group, as only one of the field can be set.
      // Note that field_count() returns how many fields in this oneof we have
      // seen so far. field_count() > 0 guarantees that i > 0, so field(i-1) is
      // safe.
      if (oneof_decl->field_count() > 0 &&
          message->field(i - 1)->containing_oneof() != oneof_decl) {
        AddError(message->full_name() + "." + message->field(i - 1)->name(),
                 proto.field(i - 1), DescriptorPool::ErrorCollector::TYPE,
                 strings::Substitute(
                     "Fields in the same oneof must be defined consecutively. "
                     "\"$0\" cannot be defined before the completion of the "
                     "\"$1\" oneof definition.",
                     message->field(i - 1)->name(), oneof_decl->name()));
      }
      // Must go through oneof_decls_ array to get a non-const version of the
      // OneofDescriptor.
      ++message->oneof_decls_[oneof_decl->index()].field_count_;
    }
  }

  // Then allocate the arrays.
  for (int i = 0; i < message->oneof_decl_count(); i++) {
    OneofDescriptor* oneof_decl = &message->oneof_decls_[i];

    if (oneof_decl->field_count() == 0) {
      AddError(message->full_name() + "." + oneof_decl->name(),
               proto.oneof_decl(i), DescriptorPool::ErrorCollector::NAME,
               "Oneof must have at least one field.");
    }

    oneof_decl->fields_ = tables_->AllocateArray<const FieldDescriptor*>(
        oneof_decl->field_count_);
    oneof_decl->field_count_ = 0;

    if (oneof_decl->options_ == nullptr) {
      oneof_decl->options_ = &OneofOptions::default_instance();
    }
  }

  // Then fill them in.
  for (int i = 0; i < message->field_count(); i++) {
    const OneofDescriptor* oneof_decl = message->field(i)->containing_oneof();
    if (oneof_decl != nullptr) {
      OneofDescriptor* mutable_oneof_decl =
          &message->oneof_decls_[oneof_decl->index()];
      message->fields_[i].index_in_oneof_ = mutable_oneof_decl->field_count_;
      mutable_oneof_decl->fields_[mutable_oneof_decl->field_count_++] =
          message->field(i);
    }
  }
}

void DescriptorBuilder::CrossLinkExtensionRange(
    Descriptor::ExtensionRange* range,
    const DescriptorProto::ExtensionRange& proto) {
  if (range->options_ == nullptr) {
    range->options_ = &ExtensionRangeOptions::default_instance();
  }
}

void DescriptorBuilder::CrossLinkField(FieldDescriptor* field,
                                       const FieldDescriptorProto& proto) {
  if (field->options_ == nullptr) {
    field->options_ = &FieldOptions::default_instance();
  }

  // Add the field to the lowercase-name and camelcase-name tables.
  file_tables_->AddFieldByStylizedNames(field);

  if (proto.has_extendee()) {
    Symbol extendee =
        LookupSymbol(proto.extendee(), field->full_name(),
                     DescriptorPool::PLACEHOLDER_EXTENDABLE_MESSAGE);
    if (extendee.IsNull()) {
      AddNotDefinedError(field->full_name(), proto,
                         DescriptorPool::ErrorCollector::EXTENDEE,
                         proto.extendee());
      return;
    } else if (extendee.type != Symbol::MESSAGE) {
      AddError(field->full_name(), proto,
               DescriptorPool::ErrorCollector::EXTENDEE,
               "\"" + proto.extendee() + "\" is not a message type.");
      return;
    }
    field->containing_type_ = extendee.descriptor;

    const Descriptor::ExtensionRange* extension_range =
        field->containing_type()->FindExtensionRangeContainingNumber(
            field->number());

    if (extension_range == nullptr) {
      AddError(field->full_name(), proto,
               DescriptorPool::ErrorCollector::NUMBER,
               strings::Substitute("\"$0\" does not declare $1 as an "
                                   "extension number.",
                                   field->containing_type()->full_name(),
                                   field->number()));
    }
  }

  if (field->containing_oneof() != nullptr) {
    if (field->label() != FieldDescriptor::LABEL_OPTIONAL) {
      // Note that this error will never happen when parsing .proto files.
      // It can only happen if you manually construct a FileDescriptorProto
      // that is incorrect.
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::NAME,
               "Fields of oneofs must themselves have label LABEL_OPTIONAL.");
    }
  }

  if (proto.has_type_name()) {
    // Assume we are expecting a message type unless the proto contains some
    // evidence that it expects an enum type.  This only makes a difference if
    // we end up creating a placeholder.
    bool expecting_enum = (proto.type() == FieldDescriptorProto::TYPE_ENUM) ||
                          proto.has_default_value();

    // In case of weak fields we force building the dependency. We need to know
    // if the type exist or not. If it doesnt exist we substitute Empty which
    // should only be done if the type can't be found in the generated pool.
    // TODO(gerbens) Ideally we should query the database directly to check
    // if weak fields exist or not so that we don't need to force building
    // weak dependencies. However the name lookup rules for symbols are
    // somewhat complicated, so I defer it too another CL.
    bool is_weak = !pool_->enforce_weak_ && proto.options().weak();
    bool is_lazy = pool_->lazily_build_dependencies_ && !is_weak;

    Symbol type =
        LookupSymbol(proto.type_name(), field->full_name(),
                     expecting_enum ? DescriptorPool::PLACEHOLDER_ENUM
                                    : DescriptorPool::PLACEHOLDER_MESSAGE,
                     LOOKUP_TYPES, !is_lazy);

    if (type.IsNull()) {
      if (is_lazy) {
        // Save the symbol names for later for lookup, and allocate the once
        // object needed for the accessors.
        std::string name = proto.type_name();
        field->type_once_ = tables_->AllocateOnceDynamic();
        field->type_name_ = tables_->AllocateString(name);
        if (proto.has_default_value()) {
          field->default_value_enum_name_ =
              tables_->AllocateString(proto.default_value());
        }
        // AddFieldByNumber and AddExtension are done later in this function,
        // and can/must be done if the field type was not found. The related
        // error checking is not necessary when in lazily_build_dependencies_
        // mode, and can't be done without building the type's descriptor,
        // which we don't want to do.
        file_tables_->AddFieldByNumber(field);
        if (field->is_extension()) {
          tables_->AddExtension(field);
        }
        return;
      } else {
        // If the type is a weak type, we change the type to a google.protobuf.Empty
        // field.
        if (is_weak) {
          type = FindSymbol(kNonLinkedWeakMessageReplacementName);
        }
        if (type.IsNull()) {
          AddNotDefinedError(field->full_name(), proto,
                             DescriptorPool::ErrorCollector::TYPE,
                             proto.type_name());
          return;
        }
      }
    }

    if (!proto.has_type()) {
      // Choose field type based on symbol.
      if (type.type == Symbol::MESSAGE) {
        field->type_ = FieldDescriptor::TYPE_MESSAGE;
      } else if (type.type == Symbol::ENUM) {
        field->type_ = FieldDescriptor::TYPE_ENUM;
      } else {
        AddError(field->full_name(), proto,
                 DescriptorPool::ErrorCollector::TYPE,
                 "\"" + proto.type_name() + "\" is not a type.");
        return;
      }
    }

    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      if (type.type != Symbol::MESSAGE) {
        AddError(field->full_name(), proto,
                 DescriptorPool::ErrorCollector::TYPE,
                 "\"" + proto.type_name() + "\" is not a message type.");
        return;
      }
      field->message_type_ = type.descriptor;

      if (field->has_default_value()) {
        AddError(field->full_name(), proto,
                 DescriptorPool::ErrorCollector::DEFAULT_VALUE,
                 "Messages can't have default values.");
      }
    } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
      if (type.type != Symbol::ENUM) {
        AddError(field->full_name(), proto,
                 DescriptorPool::ErrorCollector::TYPE,
                 "\"" + proto.type_name() + "\" is not an enum type.");
        return;
      }
      field->enum_type_ = type.enum_descriptor;

      if (field->enum_type()->is_placeholder_) {
        // We can't look up default values for placeholder types.  We'll have
        // to just drop them.
        field->has_default_value_ = false;
      }

      if (field->has_default_value()) {
        // Ensure that the default value is an identifier. Parser cannot always
        // verify this because it does not have complete type information.
        // N.B. that this check yields better error messages but is not
        // necessary for correctness (an enum symbol must be a valid identifier
        // anyway), only for better errors.
        if (!io::Tokenizer::IsIdentifier(proto.default_value())) {
          AddError(field->full_name(), proto,
                   DescriptorPool::ErrorCollector::DEFAULT_VALUE,
                   "Default value for an enum field must be an identifier.");
        } else {
          // We can't just use field->enum_type()->FindValueByName() here
          // because that locks the pool's mutex, which we have already locked
          // at this point.
          Symbol default_value = LookupSymbolNoPlaceholder(
              proto.default_value(), field->enum_type()->full_name());

          if (default_value.type == Symbol::ENUM_VALUE &&
              default_value.enum_value_descriptor->type() ==
                  field->enum_type()) {
            field->default_value_enum_ = default_value.enum_value_descriptor;
          } else {
            AddError(field->full_name(), proto,
                     DescriptorPool::ErrorCollector::DEFAULT_VALUE,
                     "Enum type \"" + field->enum_type()->full_name() +
                         "\" has no value named \"" + proto.default_value() +
                         "\".");
          }
        }
      } else if (field->enum_type()->value_count() > 0) {
        // All enums must have at least one value, or we would have reported
        // an error elsewhere.  We use the first defined value as the default
        // if a default is not explicitly defined.
        field->default_value_enum_ = field->enum_type()->value(0);
      }
    } else {
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "Field with primitive type has type_name.");
    }
  } else {
    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE ||
        field->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "Field with message or enum type missing type_name.");
    }
  }

  // Add the field to the fields-by-number table.
  // Note:  We have to do this *after* cross-linking because extensions do not
  // know their containing type until now. If we're in
  // lazily_build_dependencies_ mode, we're guaranteed there's no errors, so no
  // risk to calling containing_type() or other accessors that will build
  // dependencies.
  if (!file_tables_->AddFieldByNumber(field)) {
    const FieldDescriptor* conflicting_field = file_tables_->FindFieldByNumber(
        field->containing_type(), field->number());
    std::string containing_type_name =
        field->containing_type() == nullptr
            ? "unknown"
            : field->containing_type()->full_name();
    if (field->is_extension()) {
      AddError(field->full_name(), proto,
               DescriptorPool::ErrorCollector::NUMBER,
               strings::Substitute("Extension number $0 has already been used "
                                   "in \"$1\" by extension \"$2\".",
                                   field->number(), containing_type_name,
                                   conflicting_field->full_name()));
    } else {
      AddError(field->full_name(), proto,
               DescriptorPool::ErrorCollector::NUMBER,
               strings::Substitute("Field number $0 has already been used in "
                                   "\"$1\" by field \"$2\".",
                                   field->number(), containing_type_name,
                                   conflicting_field->name()));
    }
  } else {
    if (field->is_extension()) {
      if (!tables_->AddExtension(field)) {
        const FieldDescriptor* conflicting_field =
            tables_->FindExtension(field->containing_type(), field->number());
        std::string containing_type_name =
            field->containing_type() == nullptr
                ? "unknown"
                : field->containing_type()->full_name();
        std::string error_msg = strings::Substitute(
            "Extension number $0 has already been used in \"$1\" by extension "
            "\"$2\" defined in $3.",
            field->number(), containing_type_name,
            conflicting_field->full_name(), conflicting_field->file()->name());
        // Conflicting extension numbers should be an error. However, before
        // turning this into an error we need to fix all existing broken
        // protos first.
        // TODO(xiaofeng): Change this to an error.
        AddWarning(field->full_name(), proto,
                   DescriptorPool::ErrorCollector::NUMBER, error_msg);
      }
    }
  }
}

void DescriptorBuilder::CrossLinkEnum(EnumDescriptor* enum_type,
                                      const EnumDescriptorProto& proto) {
  if (enum_type->options_ == nullptr) {
    enum_type->options_ = &EnumOptions::default_instance();
  }

  for (int i = 0; i < enum_type->value_count(); i++) {
    CrossLinkEnumValue(&enum_type->values_[i], proto.value(i));
  }
}

void DescriptorBuilder::CrossLinkEnumValue(
    EnumValueDescriptor* enum_value,
    const EnumValueDescriptorProto& /* proto */) {
  if (enum_value->options_ == nullptr) {
    enum_value->options_ = &EnumValueOptions::default_instance();
  }
}

void DescriptorBuilder::CrossLinkService(ServiceDescriptor* service,
                                         const ServiceDescriptorProto& proto) {
  if (service->options_ == nullptr) {
    service->options_ = &ServiceOptions::default_instance();
  }

  for (int i = 0; i < service->method_count(); i++) {
    CrossLinkMethod(&service->methods_[i], proto.method(i));
  }
}

void DescriptorBuilder::CrossLinkMethod(MethodDescriptor* method,
                                        const MethodDescriptorProto& proto) {
  if (method->options_ == nullptr) {
    method->options_ = &MethodOptions::default_instance();
  }

  Symbol input_type =
      LookupSymbol(proto.input_type(), method->full_name(),
                   DescriptorPool::PLACEHOLDER_MESSAGE, LOOKUP_ALL,
                   !pool_->lazily_build_dependencies_);
  if (input_type.IsNull()) {
    if (!pool_->lazily_build_dependencies_) {
      AddNotDefinedError(method->full_name(), proto,
                         DescriptorPool::ErrorCollector::INPUT_TYPE,
                         proto.input_type());
    } else {
      method->input_type_.SetLazy(proto.input_type(), file_);
    }
  } else if (input_type.type != Symbol::MESSAGE) {
    AddError(method->full_name(), proto,
             DescriptorPool::ErrorCollector::INPUT_TYPE,
             "\"" + proto.input_type() + "\" is not a message type.");
  } else {
    method->input_type_.Set(input_type.descriptor);
  }

  Symbol output_type =
      LookupSymbol(proto.output_type(), method->full_name(),
                   DescriptorPool::PLACEHOLDER_MESSAGE, LOOKUP_ALL,
                   !pool_->lazily_build_dependencies_);
  if (output_type.IsNull()) {
    if (!pool_->lazily_build_dependencies_) {
      AddNotDefinedError(method->full_name(), proto,
                         DescriptorPool::ErrorCollector::OUTPUT_TYPE,
                         proto.output_type());
    } else {
      method->output_type_.SetLazy(proto.output_type(), file_);
    }
  } else if (output_type.type != Symbol::MESSAGE) {
    AddError(method->full_name(), proto,
             DescriptorPool::ErrorCollector::OUTPUT_TYPE,
             "\"" + proto.output_type() + "\" is not a message type.");
  } else {
    method->output_type_.Set(output_type.descriptor);
  }
}

// -------------------------------------------------------------------

#define VALIDATE_OPTIONS_FROM_ARRAY(descriptor, array_name, type) \
  for (int i = 0; i < descriptor->array_name##_count(); ++i) {    \
    Validate##type##Options(descriptor->array_name##s_ + i,       \
                            proto.array_name(i));                 \
  }

// Determine if the file uses optimize_for = LITE_RUNTIME, being careful to
// avoid problems that exist at init time.
static bool IsLite(const FileDescriptor* file) {
  // TODO(kenton):  I don't even remember how many of these conditions are
  //   actually possible.  I'm just being super-safe.
  return file != nullptr &&
         &file->options() != &FileOptions::default_instance() &&
         file->options().optimize_for() == FileOptions::LITE_RUNTIME;
}

void DescriptorBuilder::ValidateFileOptions(FileDescriptor* file,
                                            const FileDescriptorProto& proto) {
  VALIDATE_OPTIONS_FROM_ARRAY(file, message_type, Message);
  VALIDATE_OPTIONS_FROM_ARRAY(file, enum_type, Enum);
  VALIDATE_OPTIONS_FROM_ARRAY(file, service, Service);
  VALIDATE_OPTIONS_FROM_ARRAY(file, extension, Field);

  // Lite files can only be imported by other Lite files.
  if (!IsLite(file)) {
    for (int i = 0; i < file->dependency_count(); i++) {
      if (IsLite(file->dependency(i))) {
        AddError(
            file->dependency(i)->name(), proto,
            DescriptorPool::ErrorCollector::IMPORT,
            "Files that do not use optimize_for = LITE_RUNTIME cannot import "
            "files which do use this option.  This file is not lite, but it "
            "imports \"" +
                file->dependency(i)->name() + "\" which is.");
        break;
      }
    }
  }
  if (file->syntax() == FileDescriptor::SYNTAX_PROTO3) {
    ValidateProto3(file, proto);
  }
}

void DescriptorBuilder::ValidateProto3(FileDescriptor* file,
                                       const FileDescriptorProto& proto) {
  for (int i = 0; i < file->extension_count(); ++i) {
    ValidateProto3Field(file->extensions_ + i, proto.extension(i));
  }
  for (int i = 0; i < file->message_type_count(); ++i) {
    ValidateProto3Message(file->message_types_ + i, proto.message_type(i));
  }
  for (int i = 0; i < file->enum_type_count(); ++i) {
    ValidateProto3Enum(file->enum_types_ + i, proto.enum_type(i));
  }
}

static std::string ToLowercaseWithoutUnderscores(const std::string& name) {
  std::string result;
  for (int i = 0; i < name.size(); ++i) {
    if (name[i] != '_') {
      if (name[i] >= 'A' && name[i] <= 'Z') {
        result.push_back(name[i] - 'A' + 'a');
      } else {
        result.push_back(name[i]);
      }
    }
  }
  return result;
}

void DescriptorBuilder::ValidateProto3Message(Descriptor* message,
                                              const DescriptorProto& proto) {
  for (int i = 0; i < message->nested_type_count(); ++i) {
    ValidateProto3Message(message->nested_types_ + i, proto.nested_type(i));
  }
  for (int i = 0; i < message->enum_type_count(); ++i) {
    ValidateProto3Enum(message->enum_types_ + i, proto.enum_type(i));
  }
  for (int i = 0; i < message->field_count(); ++i) {
    ValidateProto3Field(message->fields_ + i, proto.field(i));
  }
  for (int i = 0; i < message->extension_count(); ++i) {
    ValidateProto3Field(message->extensions_ + i, proto.extension(i));
  }
  if (message->extension_range_count() > 0) {
    AddError(message->full_name(), proto.extension_range(0),
             DescriptorPool::ErrorCollector::NUMBER,
             "Extension ranges are not allowed in proto3.");
  }
  if (message->options().message_set_wire_format()) {
    // Using MessageSet doesn't make sense since we disallow extensions.
    AddError(message->full_name(), proto, DescriptorPool::ErrorCollector::NAME,
             "MessageSet is not supported in proto3.");
  }

  // In proto3, we reject field names if they conflict in camelCase.
  // Note that we currently enforce a stricter rule: Field names must be
  // unique after being converted to lowercase with underscores removed.
  std::map<std::string, const FieldDescriptor*> name_to_field;
  for (int i = 0; i < message->field_count(); ++i) {
    std::string lowercase_name =
        ToLowercaseWithoutUnderscores(message->field(i)->name());
    if (name_to_field.find(lowercase_name) != name_to_field.end()) {
      AddError(message->full_name(), proto.field(i),
               DescriptorPool::ErrorCollector::NAME,
               "The JSON camel-case name of field \"" +
                   message->field(i)->name() + "\" conflicts with field \"" +
                   name_to_field[lowercase_name]->name() + "\". This is not " +
                   "allowed in proto3.");
    } else {
      name_to_field[lowercase_name] = message->field(i);
    }
  }
}

void DescriptorBuilder::ValidateProto3Field(FieldDescriptor* field,
                                            const FieldDescriptorProto& proto) {
  if (field->is_extension() &&
      !AllowedExtendeeInProto3(field->containing_type()->full_name())) {
    AddError(field->full_name(), proto,
             DescriptorPool::ErrorCollector::EXTENDEE,
             "Extensions in proto3 are only allowed for defining options.");
  }
  if (field->is_required()) {
    AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
             "Required fields are not allowed in proto3.");
  }
  if (field->has_default_value()) {
    AddError(field->full_name(), proto,
             DescriptorPool::ErrorCollector::DEFAULT_VALUE,
             "Explicit default values are not allowed in proto3.");
  }
  if (field->cpp_type() == FieldDescriptor::CPPTYPE_ENUM &&
      field->enum_type() &&
      field->enum_type()->file()->syntax() != FileDescriptor::SYNTAX_PROTO3) {
    // Proto3 messages can only use Proto3 enum types; otherwise we can't
    // guarantee that the default value is zero.
    AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
             "Enum type \"" + field->enum_type()->full_name() +
                 "\" is not a proto3 enum, but is used in \"" +
                 field->containing_type()->full_name() +
                 "\" which is a proto3 message type.");
  }
  if (field->type() == FieldDescriptor::TYPE_GROUP) {
    AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
             "Groups are not supported in proto3 syntax.");
  }
}

void DescriptorBuilder::ValidateProto3Enum(EnumDescriptor* enm,
                                           const EnumDescriptorProto& proto) {
  if (enm->value_count() > 0 && enm->value(0)->number() != 0) {
    AddError(enm->full_name(), proto.value(0),
             DescriptorPool::ErrorCollector::NUMBER,
             "The first enum value must be zero in proto3.");
  }
}

void DescriptorBuilder::ValidateMessageOptions(Descriptor* message,
                                               const DescriptorProto& proto) {
  VALIDATE_OPTIONS_FROM_ARRAY(message, field, Field);
  VALIDATE_OPTIONS_FROM_ARRAY(message, nested_type, Message);
  VALIDATE_OPTIONS_FROM_ARRAY(message, enum_type, Enum);
  VALIDATE_OPTIONS_FROM_ARRAY(message, extension, Field);

  const int64 max_extension_range =
      static_cast<int64>(message->options().message_set_wire_format()
                             ? kint32max
                             : FieldDescriptor::kMaxNumber);
  for (int i = 0; i < message->extension_range_count(); ++i) {
    if (message->extension_range(i)->end > max_extension_range + 1) {
      AddError(
          message->full_name(), proto.extension_range(i),
          DescriptorPool::ErrorCollector::NUMBER,
          strings::Substitute("Extension numbers cannot be greater than $0.",
                              max_extension_range));
    }
  }
}


void DescriptorBuilder::ValidateFieldOptions(
    FieldDescriptor* field, const FieldDescriptorProto& proto) {
  if (pool_->lazily_build_dependencies_ && (!field || !field->message_type())) {
    return;
  }
  // Only message type fields may be lazy.
  if (field->options().lazy()) {
    if (field->type() != FieldDescriptor::TYPE_MESSAGE) {
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "[lazy = true] can only be specified for submessage fields.");
    }
  }

  // Only repeated primitive fields may be packed.
  if (field->options().packed() && !field->is_packable()) {
    AddError(
        field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
        "[packed = true] can only be specified for repeated primitive fields.");
  }

  // Note:  Default instance may not yet be initialized here, so we have to
  //   avoid reading from it.
  if (field->containing_type_ != nullptr &&
      &field->containing_type()->options() !=
          &MessageOptions::default_instance() &&
      field->containing_type()->options().message_set_wire_format()) {
    if (field->is_extension()) {
      if (!field->is_optional() ||
          field->type() != FieldDescriptor::TYPE_MESSAGE) {
        AddError(field->full_name(), proto,
                 DescriptorPool::ErrorCollector::TYPE,
                 "Extensions of MessageSets must be optional messages.");
      }
    } else {
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::NAME,
               "MessageSets cannot have fields, only extensions.");
    }
  }

  // Lite extensions can only be of Lite types.
  if (IsLite(field->file()) && field->containing_type_ != nullptr &&
      !IsLite(field->containing_type()->file())) {
    AddError(field->full_name(), proto,
             DescriptorPool::ErrorCollector::EXTENDEE,
             "Extensions to non-lite types can only be declared in non-lite "
             "files.  Note that you cannot extend a non-lite type to contain "
             "a lite type, but the reverse is allowed.");
  }

  // Validate map types.
  if (field->is_map()) {
    if (!ValidateMapEntry(field, proto)) {
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "map_entry should not be set explicitly. Use map<KeyType, "
               "ValueType> instead.");
    }
  }

  ValidateJSType(field, proto);

  // json_name option is not allowed on extension fields. Note that the
  // json_name field in FieldDescriptorProto is always populated by protoc
  // when it sends descriptor data to plugins (caculated from field name if
  // the option is not explicitly set) so we can't rely on its presence to
  // determine whether the json_name option is set on the field. Here we
  // compare it against the default calculated json_name value and consider
  // the option set if they are different. This won't catch the case when
  // an user explicitly sets json_name to the default value, but should be
  // good enough to catch common misuses.
  if (field->is_extension() &&
      (field->has_json_name() &&
       field->json_name() != ToJsonName(field->name()))) {
    AddError(field->full_name(), proto,
             DescriptorPool::ErrorCollector::OPTION_NAME,
             "option json_name is not allowed on extension fields.");
  }

}

void DescriptorBuilder::ValidateEnumOptions(EnumDescriptor* enm,
                                            const EnumDescriptorProto& proto) {
  VALIDATE_OPTIONS_FROM_ARRAY(enm, value, EnumValue);
  if (!enm->options().has_allow_alias() || !enm->options().allow_alias()) {
    std::map<int, std::string> used_values;
    for (int i = 0; i < enm->value_count(); ++i) {
      const EnumValueDescriptor* enum_value = enm->value(i);
      if (used_values.find(enum_value->number()) != used_values.end()) {
        std::string error =
            "\"" + enum_value->full_name() +
            "\" uses the same enum value as \"" +
            used_values[enum_value->number()] +
            "\". If this is intended, set "
            "'option allow_alias = true;' to the enum definition.";
        if (!enm->options().allow_alias()) {
          // Generate error if duplicated enum values are explicitly disallowed.
          AddError(enm->full_name(), proto.value(i),
                   DescriptorPool::ErrorCollector::NUMBER, error);
        }
      } else {
        used_values[enum_value->number()] = enum_value->full_name();
      }
    }
  }
}

void DescriptorBuilder::ValidateEnumValueOptions(
    EnumValueDescriptor* /* enum_value */,
    const EnumValueDescriptorProto& /* proto */) {
  // Nothing to do so far.
}
void DescriptorBuilder::ValidateServiceOptions(
    ServiceDescriptor* service, const ServiceDescriptorProto& proto) {
  if (IsLite(service->file()) &&
      (service->file()->options().cc_generic_services() ||
       service->file()->options().java_generic_services())) {
    AddError(service->full_name(), proto, DescriptorPool::ErrorCollector::NAME,
             "Files with optimize_for = LITE_RUNTIME cannot define services "
             "unless you set both options cc_generic_services and "
             "java_generic_services to false.");
  }

  VALIDATE_OPTIONS_FROM_ARRAY(service, method, Method);
}

void DescriptorBuilder::ValidateMethodOptions(
    MethodDescriptor* /* method */, const MethodDescriptorProto& /* proto */) {
  // Nothing to do so far.
}

bool DescriptorBuilder::ValidateMapEntry(FieldDescriptor* field,
                                         const FieldDescriptorProto& proto) {
  const Descriptor* message = field->message_type();
  if (  // Must not contain extensions, extension range or nested message or
        // enums
      message->extension_count() != 0 ||
      field->label() != FieldDescriptor::LABEL_REPEATED ||
      message->extension_range_count() != 0 ||
      message->nested_type_count() != 0 || message->enum_type_count() != 0 ||
      // Must contain exactly two fields
      message->field_count() != 2 ||
      // Field name and message name must match
      message->name() != ToCamelCase(field->name(), false) + "Entry" ||
      // Entry message must be in the same containing type of the field.
      field->containing_type() != message->containing_type()) {
    return false;
  }

  const FieldDescriptor* key = message->field(0);
  const FieldDescriptor* value = message->field(1);
  if (key->label() != FieldDescriptor::LABEL_OPTIONAL || key->number() != 1 ||
      key->name() != "key") {
    return false;
  }
  if (value->label() != FieldDescriptor::LABEL_OPTIONAL ||
      value->number() != 2 || value->name() != "value") {
    return false;
  }

  // Check key types are legal.
  switch (key->type()) {
    case FieldDescriptor::TYPE_ENUM:
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "Key in map fields cannot be enum types.");
      break;
    case FieldDescriptor::TYPE_FLOAT:
    case FieldDescriptor::TYPE_DOUBLE:
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_BYTES:
      AddError(
          field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
          "Key in map fields cannot be float/double, bytes or message types.");
      break;
    case FieldDescriptor::TYPE_BOOL:
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_STRING:
    case FieldDescriptor::TYPE_UINT32:
    case FieldDescriptor::TYPE_UINT64:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_SFIXED64:
      // Legal cases
      break;
      // Do not add a default, so that the compiler will complain when new types
      // are added.
  }

  if (value->type() == FieldDescriptor::TYPE_ENUM) {
    if (value->enum_type()->value(0)->number() != 0) {
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "Enum value in map must define 0 as the first value.");
    }
  }

  return true;
}

void DescriptorBuilder::DetectMapConflicts(const Descriptor* message,
                                           const DescriptorProto& proto) {
  std::map<std::string, const Descriptor*> seen_types;
  for (int i = 0; i < message->nested_type_count(); ++i) {
    const Descriptor* nested = message->nested_type(i);
    std::pair<std::map<std::string, const Descriptor*>::iterator, bool> result =
        seen_types.insert(std::make_pair(nested->name(), nested));
    if (!result.second) {
      if (result.first->second->options().map_entry() ||
          nested->options().map_entry()) {
        AddError(message->full_name(), proto,
                 DescriptorPool::ErrorCollector::NAME,
                 "Expanded map entry type " + nested->name() +
                     " conflicts with an existing nested message type.");
      }
    }
    // Recursively test on the nested types.
    DetectMapConflicts(message->nested_type(i), proto.nested_type(i));
  }
  // Check for conflicted field names.
  for (int i = 0; i < message->field_count(); ++i) {
    const FieldDescriptor* field = message->field(i);
    std::map<std::string, const Descriptor*>::iterator iter =
        seen_types.find(field->name());
    if (iter != seen_types.end() && iter->second->options().map_entry()) {
      AddError(message->full_name(), proto,
               DescriptorPool::ErrorCollector::NAME,
               "Expanded map entry type " + iter->second->name() +
                   " conflicts with an existing field.");
    }
  }
  // Check for conflicted enum names.
  for (int i = 0; i < message->enum_type_count(); ++i) {
    const EnumDescriptor* enum_desc = message->enum_type(i);
    std::map<std::string, const Descriptor*>::iterator iter =
        seen_types.find(enum_desc->name());
    if (iter != seen_types.end() && iter->second->options().map_entry()) {
      AddError(message->full_name(), proto,
               DescriptorPool::ErrorCollector::NAME,
               "Expanded map entry type " + iter->second->name() +
                   " conflicts with an existing enum type.");
    }
  }
  // Check for conflicted oneof names.
  for (int i = 0; i < message->oneof_decl_count(); ++i) {
    const OneofDescriptor* oneof_desc = message->oneof_decl(i);
    std::map<std::string, const Descriptor*>::iterator iter =
        seen_types.find(oneof_desc->name());
    if (iter != seen_types.end() && iter->second->options().map_entry()) {
      AddError(message->full_name(), proto,
               DescriptorPool::ErrorCollector::NAME,
               "Expanded map entry type " + iter->second->name() +
                   " conflicts with an existing oneof type.");
    }
  }
}

void DescriptorBuilder::ValidateJSType(FieldDescriptor* field,
                                       const FieldDescriptorProto& proto) {
  FieldOptions::JSType jstype = field->options().jstype();
  // The default is always acceptable.
  if (jstype == FieldOptions::JS_NORMAL) {
    return;
  }

  switch (field->type()) {
    // Integral 64-bit types may be represented as JavaScript numbers or
    // strings.
    case FieldDescriptor::TYPE_UINT64:
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SFIXED64:
      if (jstype == FieldOptions::JS_STRING ||
          jstype == FieldOptions::JS_NUMBER) {
        return;
      }
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "Illegal jstype for int64, uint64, sint64, fixed64 "
               "or sfixed64 field: " +
                   FieldOptions_JSType_descriptor()->value(jstype)->name());
      break;

    // No other types permit a jstype option.
    default:
      AddError(field->full_name(), proto, DescriptorPool::ErrorCollector::TYPE,
               "jstype is only allowed on int64, uint64, sint64, fixed64 "
               "or sfixed64 fields.");
      break;
  }
}

#undef VALIDATE_OPTIONS_FROM_ARRAY

// -------------------------------------------------------------------

DescriptorBuilder::OptionInterpreter::OptionInterpreter(
    DescriptorBuilder* builder)
    : builder_(builder) {
  GOOGLE_CHECK(builder_);
}

DescriptorBuilder::OptionInterpreter::~OptionInterpreter() {}

bool DescriptorBuilder::OptionInterpreter::InterpretOptions(
    OptionsToInterpret* options_to_interpret) {
  // Note that these may be in different pools, so we can't use the same
  // descriptor and reflection objects on both.
  Message* options = options_to_interpret->options;
  const Message* original_options = options_to_interpret->original_options;

  bool failed = false;
  options_to_interpret_ = options_to_interpret;

  // Find the uninterpreted_option field in the mutable copy of the options
  // and clear them, since we're about to interpret them.
  const FieldDescriptor* uninterpreted_options_field =
      options->GetDescriptor()->FindFieldByName("uninterpreted_option");
  GOOGLE_CHECK(uninterpreted_options_field != nullptr)
      << "No field named \"uninterpreted_option\" in the Options proto.";
  options->GetReflection()->ClearField(options, uninterpreted_options_field);

  std::vector<int> src_path = options_to_interpret->element_path;
  src_path.push_back(uninterpreted_options_field->number());

  // Find the uninterpreted_option field in the original options.
  const FieldDescriptor* original_uninterpreted_options_field =
      original_options->GetDescriptor()->FindFieldByName(
          "uninterpreted_option");
  GOOGLE_CHECK(original_uninterpreted_options_field != nullptr)
      << "No field named \"uninterpreted_option\" in the Options proto.";

  const int num_uninterpreted_options =
      original_options->GetReflection()->FieldSize(
          *original_options, original_uninterpreted_options_field);
  for (int i = 0; i < num_uninterpreted_options; ++i) {
    src_path.push_back(i);
    uninterpreted_option_ = down_cast<const UninterpretedOption*>(
        &original_options->GetReflection()->GetRepeatedMessage(
            *original_options, original_uninterpreted_options_field, i));
    if (!InterpretSingleOption(options, src_path,
                               options_to_interpret->element_path)) {
      // Error already added by InterpretSingleOption().
      failed = true;
      break;
    }
    src_path.pop_back();
  }
  // Reset these, so we don't have any dangling pointers.
  uninterpreted_option_ = nullptr;
  options_to_interpret_ = nullptr;

  if (!failed) {
    // InterpretSingleOption() added the interpreted options in the
    // UnknownFieldSet, in case the option isn't yet known to us.  Now we
    // serialize the options message and deserialize it back.  That way, any
    // option fields that we do happen to know about will get moved from the
    // UnknownFieldSet into the real fields, and thus be available right away.
    // If they are not known, that's OK too. They will get reparsed into the
    // UnknownFieldSet and wait there until the message is parsed by something
    // that does know about the options.

    // Keep the unparsed options around in case the reparsing fails.
    std::unique_ptr<Message> unparsed_options(options->New());
    options->GetReflection()->Swap(unparsed_options.get(), options);

    std::string buf;
    if (!unparsed_options->AppendToString(&buf) ||
        !options->ParseFromString(buf)) {
      builder_->AddError(
          options_to_interpret->element_name, *original_options,
          DescriptorPool::ErrorCollector::OTHER,
          "Some options could not be correctly parsed using the proto "
          "descriptors compiled into this binary.\n"
          "Unparsed options: " +
              unparsed_options->ShortDebugString() +
              "\n"
              "Parsing attempt:  " +
              options->ShortDebugString());
      // Restore the unparsed options.
      options->GetReflection()->Swap(unparsed_options.get(), options);
    }
  }
  return !failed;
}

bool DescriptorBuilder::OptionInterpreter::InterpretSingleOption(
    Message* options, const std::vector<int>& src_path,
    const std::vector<int>& options_path) {
  // First do some basic validation.
  if (uninterpreted_option_->name_size() == 0) {
    // This should never happen unless the parser has gone seriously awry or
    // someone has manually created the uninterpreted option badly.
    return AddNameError("Option must have a name.");
  }
  if (uninterpreted_option_->name(0).name_part() == "uninterpreted_option") {
    return AddNameError(
        "Option must not use reserved name "
        "\"uninterpreted_option\".");
  }

  const Descriptor* options_descriptor = nullptr;
  // Get the options message's descriptor from the builder's pool, so that we
  // get the version that knows about any extension options declared in the file
  // we're currently building. The descriptor should be there as long as the
  // file we're building imported descriptor.proto.

  // Note that we use DescriptorBuilder::FindSymbolNotEnforcingDeps(), not
  // DescriptorPool::FindMessageTypeByName() because we're already holding the
  // pool's mutex, and the latter method locks it again.  We don't use
  // FindSymbol() because files that use custom options only need to depend on
  // the file that defines the option, not descriptor.proto itself.
  Symbol symbol = builder_->FindSymbolNotEnforcingDeps(
      options->GetDescriptor()->full_name());
  if (!symbol.IsNull() && symbol.type == Symbol::MESSAGE) {
    options_descriptor = symbol.descriptor;
  } else {
    // The options message's descriptor was not in the builder's pool, so use
    // the standard version from the generated pool. We're not holding the
    // generated pool's mutex, so we can search it the straightforward way.
    options_descriptor = options->GetDescriptor();
  }
  GOOGLE_CHECK(options_descriptor);

  // We iterate over the name parts to drill into the submessages until we find
  // the leaf field for the option. As we drill down we remember the current
  // submessage's descriptor in |descriptor| and the next field in that
  // submessage in |field|. We also track the fields we're drilling down
  // through in |intermediate_fields|. As we go, we reconstruct the full option
  // name in |debug_msg_name|, for use in error messages.
  const Descriptor* descriptor = options_descriptor;
  const FieldDescriptor* field = nullptr;
  std::vector<const FieldDescriptor*> intermediate_fields;
  std::string debug_msg_name = "";

  std::vector<int> dest_path = options_path;

  for (int i = 0; i < uninterpreted_option_->name_size(); ++i) {
    const std::string& name_part = uninterpreted_option_->name(i).name_part();
    if (debug_msg_name.size() > 0) {
      debug_msg_name += ".";
    }
    if (uninterpreted_option_->name(i).is_extension()) {
      debug_msg_name += "(" + name_part + ")";
      // Search for the extension's descriptor as an extension in the builder's
      // pool. Note that we use DescriptorBuilder::LookupSymbol(), not
      // DescriptorPool::FindExtensionByName(), for two reasons: 1) It allows
      // relative lookups, and 2) because we're already holding the pool's
      // mutex, and the latter method locks it again.
      symbol =
          builder_->LookupSymbol(name_part, options_to_interpret_->name_scope);
      if (!symbol.IsNull() && symbol.type == Symbol::FIELD) {
        field = symbol.field_descriptor;
      }
      // If we don't find the field then the field's descriptor was not in the
      // builder's pool, but there's no point in looking in the generated
      // pool. We require that you import the file that defines any extensions
      // you use, so they must be present in the builder's pool.
    } else {
      debug_msg_name += name_part;
      // Search for the field's descriptor as a regular field.
      field = descriptor->FindFieldByName(name_part);
    }

    if (field == nullptr) {
      if (get_allow_unknown(builder_->pool_)) {
        // We can't find the option, but AllowUnknownDependencies() is enabled,
        // so we will just leave it as uninterpreted.
        AddWithoutInterpreting(*uninterpreted_option_, options);
        return true;
      } else if (!(builder_->undefine_resolved_name_).empty()) {
        // Option is resolved to a name which is not defined.
        return AddNameError(
            "Option \"" + debug_msg_name + "\" is resolved to \"(" +
            builder_->undefine_resolved_name_ +
            ")\", which is not defined. The innermost scope is searched first "
            "in name resolution. Consider using a leading '.'(i.e., \"(." +
            debug_msg_name.substr(1) +
            "\") to start from the outermost scope.");
      } else {
        return AddNameError(
            "Option \"" + debug_msg_name +
            "\" unknown. Ensure that your proto" +
            " definition file imports the proto which defines the option.");
      }
    } else if (field->containing_type() != descriptor) {
      if (get_is_placeholder(field->containing_type())) {
        // The field is an extension of a placeholder type, so we can't
        // reliably verify whether it is a valid extension to use here (e.g.
        // we don't know if it is an extension of the correct *Options message,
        // or if it has a valid field number, etc.).  Just leave it as
        // uninterpreted instead.
        AddWithoutInterpreting(*uninterpreted_option_, options);
        return true;
      } else {
        // This can only happen if, due to some insane misconfiguration of the
        // pools, we find the options message in one pool but the field in
        // another. This would probably imply a hefty bug somewhere.
        return AddNameError("Option field \"" + debug_msg_name +
                            "\" is not a field or extension of message \"" +
                            descriptor->name() + "\".");
      }
    } else {
      // accumulate field numbers to form path to interpreted option
      dest_path.push_back(field->number());

      if (i < uninterpreted_option_->name_size() - 1) {
        if (field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE) {
          return AddNameError("Option \"" + debug_msg_name +
                              "\" is an atomic type, not a message.");
        } else if (field->is_repeated()) {
          return AddNameError("Option field \"" + debug_msg_name +
                              "\" is a repeated message. Repeated message "
                              "options must be initialized using an "
                              "aggregate value.");
        } else {
          // Drill down into the submessage.
          intermediate_fields.push_back(field);
          descriptor = field->message_type();
        }
      }
    }
  }

  // We've found the leaf field. Now we use UnknownFieldSets to set its value
  // on the options message. We do so because the message may not yet know
  // about its extension fields, so we may not be able to set the fields
  // directly. But the UnknownFieldSets will serialize to the same wire-format
  // message, so reading that message back in once the extension fields are
  // known will populate them correctly.

  // First see if the option is already set.
  if (!field->is_repeated() &&
      !ExamineIfOptionIsSet(
          intermediate_fields.begin(), intermediate_fields.end(), field,
          debug_msg_name,
          options->GetReflection()->GetUnknownFields(*options))) {
    return false;  // ExamineIfOptionIsSet() already added the error.
  }

  // First set the value on the UnknownFieldSet corresponding to the
  // innermost message.
  std::unique_ptr<UnknownFieldSet> unknown_fields(new UnknownFieldSet());
  if (!SetOptionValue(field, unknown_fields.get())) {
    return false;  // SetOptionValue() already added the error.
  }

  // Now wrap the UnknownFieldSet with UnknownFieldSets corresponding to all
  // the intermediate messages.
  for (std::vector<const FieldDescriptor*>::reverse_iterator iter =
           intermediate_fields.rbegin();
       iter != intermediate_fields.rend(); ++iter) {
    std::unique_ptr<UnknownFieldSet> parent_unknown_fields(
        new UnknownFieldSet());
    switch ((*iter)->type()) {
      case FieldDescriptor::TYPE_MESSAGE: {
        io::StringOutputStream outstr(
            parent_unknown_fields->AddLengthDelimited((*iter)->number()));
        io::CodedOutputStream out(&outstr);
        internal::WireFormat::SerializeUnknownFields(*unknown_fields, &out);
        GOOGLE_CHECK(!out.HadError())
            << "Unexpected failure while serializing option submessage "
            << debug_msg_name << "\".";
        break;
      }

      case FieldDescriptor::TYPE_GROUP: {
        parent_unknown_fields->AddGroup((*iter)->number())
            ->MergeFrom(*unknown_fields);
        break;
      }

      default:
        GOOGLE_LOG(FATAL) << "Invalid wire type for CPPTYPE_MESSAGE: "
                   << (*iter)->type();
        return false;
    }
    unknown_fields.reset(parent_unknown_fields.release());
  }

  // Now merge the UnknownFieldSet corresponding to the top-level message into
  // the options message.
  options->GetReflection()->MutableUnknownFields(options)->MergeFrom(
      *unknown_fields);

  // record the element path of the interpreted option
  if (field->is_repeated()) {
    int index = repeated_option_counts_[dest_path]++;
    dest_path.push_back(index);
  }
  interpreted_paths_[src_path] = dest_path;

  return true;
}

void DescriptorBuilder::OptionInterpreter::UpdateSourceCodeInfo(
    SourceCodeInfo* info) {
  if (interpreted_paths_.empty()) {
    // nothing to do!
    return;
  }

  // We find locations that match keys in interpreted_paths_ and
  // 1) replace the path with the corresponding value in interpreted_paths_
  // 2) remove any subsequent sub-locations (sub-location is one whose path
  //    has the parent path as a prefix)
  //
  // To avoid quadratic behavior of removing interior rows as we go,
  // we keep a copy. But we don't actually copy anything until we've
  // found the first match (so if the source code info has no locations
  // that need to be changed, there is zero copy overhead).

  RepeatedPtrField<SourceCodeInfo_Location>* locs = info->mutable_location();
  RepeatedPtrField<SourceCodeInfo_Location> new_locs;
  bool copying = false;

  std::vector<int> pathv;
  bool matched = false;

  for (RepeatedPtrField<SourceCodeInfo_Location>::iterator loc = locs->begin();
       loc != locs->end(); loc++) {
    if (matched) {
      // see if this location is in the range to remove
      bool loc_matches = true;
      if (loc->path_size() < pathv.size()) {
        loc_matches = false;
      } else {
        for (int j = 0; j < pathv.size(); j++) {
          if (loc->path(j) != pathv[j]) {
            loc_matches = false;
            break;
          }
        }
      }

      if (loc_matches) {
        // don't copy this row since it is a sub-location that we're removing
        continue;
      }

      matched = false;
    }

    pathv.clear();
    for (int j = 0; j < loc->path_size(); j++) {
      pathv.push_back(loc->path(j));
    }

    std::map<std::vector<int>, std::vector<int>>::iterator entry =
        interpreted_paths_.find(pathv);

    if (entry == interpreted_paths_.end()) {
      // not a match
      if (copying) {
        *new_locs.Add() = *loc;
      }
      continue;
    }

    matched = true;

    if (!copying) {
      // initialize the copy we are building
      copying = true;
      new_locs.Reserve(locs->size());
      for (RepeatedPtrField<SourceCodeInfo_Location>::iterator it =
               locs->begin();
           it != loc; it++) {
        *new_locs.Add() = *it;
      }
    }

    // add replacement and update its path
    SourceCodeInfo_Location* replacement = new_locs.Add();
    *replacement = *loc;
    replacement->clear_path();
    for (std::vector<int>::iterator rit = entry->second.begin();
         rit != entry->second.end(); rit++) {
      replacement->add_path(*rit);
    }
  }

  // if we made a changed copy, put it in place
  if (copying) {
    *locs = new_locs;
  }
}

void DescriptorBuilder::OptionInterpreter::AddWithoutInterpreting(
    const UninterpretedOption& uninterpreted_option, Message* options) {
  const FieldDescriptor* field =
      options->GetDescriptor()->FindFieldByName("uninterpreted_option");
  GOOGLE_CHECK(field != nullptr);

  options->GetReflection()
      ->AddMessage(options, field)
      ->CopyFrom(uninterpreted_option);
}

bool DescriptorBuilder::OptionInterpreter::ExamineIfOptionIsSet(
    std::vector<const FieldDescriptor*>::const_iterator
        intermediate_fields_iter,
    std::vector<const FieldDescriptor*>::const_iterator intermediate_fields_end,
    const FieldDescriptor* innermost_field, const std::string& debug_msg_name,
    const UnknownFieldSet& unknown_fields) {
  // We do linear searches of the UnknownFieldSet and its sub-groups.  This
  // should be fine since it's unlikely that any one options structure will
  // contain more than a handful of options.

  if (intermediate_fields_iter == intermediate_fields_end) {
    // We're at the innermost submessage.
    for (int i = 0; i < unknown_fields.field_count(); i++) {
      if (unknown_fields.field(i).number() == innermost_field->number()) {
        return AddNameError("Option \"" + debug_msg_name +
                            "\" was already set.");
      }
    }
    return true;
  }

  for (int i = 0; i < unknown_fields.field_count(); i++) {
    if (unknown_fields.field(i).number() ==
        (*intermediate_fields_iter)->number()) {
      const UnknownField* unknown_field = &unknown_fields.field(i);
      FieldDescriptor::Type type = (*intermediate_fields_iter)->type();
      // Recurse into the next submessage.
      switch (type) {
        case FieldDescriptor::TYPE_MESSAGE:
          if (unknown_field->type() == UnknownField::TYPE_LENGTH_DELIMITED) {
            UnknownFieldSet intermediate_unknown_fields;
            if (intermediate_unknown_fields.ParseFromString(
                    unknown_field->length_delimited()) &&
                !ExamineIfOptionIsSet(intermediate_fields_iter + 1,
                                      intermediate_fields_end, innermost_field,
                                      debug_msg_name,
                                      intermediate_unknown_fields)) {
              return false;  // Error already added.
            }
          }
          break;

        case FieldDescriptor::TYPE_GROUP:
          if (unknown_field->type() == UnknownField::TYPE_GROUP) {
            if (!ExamineIfOptionIsSet(intermediate_fields_iter + 1,
                                      intermediate_fields_end, innermost_field,
                                      debug_msg_name, unknown_field->group())) {
              return false;  // Error already added.
            }
          }
          break;

        default:
          GOOGLE_LOG(FATAL) << "Invalid wire type for CPPTYPE_MESSAGE: " << type;
          return false;
      }
    }
  }
  return true;
}

bool DescriptorBuilder::OptionInterpreter::SetOptionValue(
    const FieldDescriptor* option_field, UnknownFieldSet* unknown_fields) {
  // We switch on the CppType to validate.
  switch (option_field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      if (uninterpreted_option_->has_positive_int_value()) {
        if (uninterpreted_option_->positive_int_value() >
            static_cast<uint64>(kint32max)) {
          return AddValueError("Value out of range for int32 option \"" +
                               option_field->full_name() + "\".");
        } else {
          SetInt32(option_field->number(),
                   uninterpreted_option_->positive_int_value(),
                   option_field->type(), unknown_fields);
        }
      } else if (uninterpreted_option_->has_negative_int_value()) {
        if (uninterpreted_option_->negative_int_value() <
            static_cast<int64>(kint32min)) {
          return AddValueError("Value out of range for int32 option \"" +
                               option_field->full_name() + "\".");
        } else {
          SetInt32(option_field->number(),
                   uninterpreted_option_->negative_int_value(),
                   option_field->type(), unknown_fields);
        }
      } else {
        return AddValueError("Value must be integer for int32 option \"" +
                             option_field->full_name() + "\".");
      }
      break;

    case FieldDescriptor::CPPTYPE_INT64:
      if (uninterpreted_option_->has_positive_int_value()) {
        if (uninterpreted_option_->positive_int_value() >
            static_cast<uint64>(kint64max)) {
          return AddValueError("Value out of range for int64 option \"" +
                               option_field->full_name() + "\".");
        } else {
          SetInt64(option_field->number(),
                   uninterpreted_option_->positive_int_value(),
                   option_field->type(), unknown_fields);
        }
      } else if (uninterpreted_option_->has_negative_int_value()) {
        SetInt64(option_field->number(),
                 uninterpreted_option_->negative_int_value(),
                 option_field->type(), unknown_fields);
      } else {
        return AddValueError("Value must be integer for int64 option \"" +
                             option_field->full_name() + "\".");
      }
      break;

    case FieldDescriptor::CPPTYPE_UINT32:
      if (uninterpreted_option_->has_positive_int_value()) {
        if (uninterpreted_option_->positive_int_value() > kuint32max) {
          return AddValueError("Value out of range for uint32 option \"" +
                               option_field->name() + "\".");
        } else {
          SetUInt32(option_field->number(),
                    uninterpreted_option_->positive_int_value(),
                    option_field->type(), unknown_fields);
        }
      } else {
        return AddValueError(
            "Value must be non-negative integer for uint32 "
            "option \"" +
            option_field->full_name() + "\".");
      }
      break;

    case FieldDescriptor::CPPTYPE_UINT64:
      if (uninterpreted_option_->has_positive_int_value()) {
        SetUInt64(option_field->number(),
                  uninterpreted_option_->positive_int_value(),
                  option_field->type(), unknown_fields);
      } else {
        return AddValueError(
            "Value must be non-negative integer for uint64 "
            "option \"" +
            option_field->full_name() + "\".");
      }
      break;

    case FieldDescriptor::CPPTYPE_FLOAT: {
      float value;
      if (uninterpreted_option_->has_double_value()) {
        value = uninterpreted_option_->double_value();
      } else if (uninterpreted_option_->has_positive_int_value()) {
        value = uninterpreted_option_->positive_int_value();
      } else if (uninterpreted_option_->has_negative_int_value()) {
        value = uninterpreted_option_->negative_int_value();
      } else {
        return AddValueError("Value must be number for float option \"" +
                             option_field->full_name() + "\".");
      }
      unknown_fields->AddFixed32(option_field->number(),
                                 internal::WireFormatLite::EncodeFloat(value));
      break;
    }

    case FieldDescriptor::CPPTYPE_DOUBLE: {
      double value;
      if (uninterpreted_option_->has_double_value()) {
        value = uninterpreted_option_->double_value();
      } else if (uninterpreted_option_->has_positive_int_value()) {
        value = uninterpreted_option_->positive_int_value();
      } else if (uninterpreted_option_->has_negative_int_value()) {
        value = uninterpreted_option_->negative_int_value();
      } else {
        return AddValueError("Value must be number for double option \"" +
                             option_field->full_name() + "\".");
      }
      unknown_fields->AddFixed64(option_field->number(),
                                 internal::WireFormatLite::EncodeDouble(value));
      break;
    }

    case FieldDescriptor::CPPTYPE_BOOL:
      uint64 value;
      if (!uninterpreted_option_->has_identifier_value()) {
        return AddValueError(
            "Value must be identifier for boolean option "
            "\"" +
            option_field->full_name() + "\".");
      }
      if (uninterpreted_option_->identifier_value() == "true") {
        value = 1;
      } else if (uninterpreted_option_->identifier_value() == "false") {
        value = 0;
      } else {
        return AddValueError(
            "Value must be \"true\" or \"false\" for boolean "
            "option \"" +
            option_field->full_name() + "\".");
      }
      unknown_fields->AddVarint(option_field->number(), value);
      break;

    case FieldDescriptor::CPPTYPE_ENUM: {
      if (!uninterpreted_option_->has_identifier_value()) {
        return AddValueError(
            "Value must be identifier for enum-valued option "
            "\"" +
            option_field->full_name() + "\".");
      }
      const EnumDescriptor* enum_type = option_field->enum_type();
      const std::string& value_name = uninterpreted_option_->identifier_value();
      const EnumValueDescriptor* enum_value = nullptr;

      if (enum_type->file()->pool() != DescriptorPool::generated_pool()) {
        // Note that the enum value's fully-qualified name is a sibling of the
        // enum's name, not a child of it.
        std::string fully_qualified_name = enum_type->full_name();
        fully_qualified_name.resize(fully_qualified_name.size() -
                                    enum_type->name().size());
        fully_qualified_name += value_name;

        // Search for the enum value's descriptor in the builder's pool. Note
        // that we use DescriptorBuilder::FindSymbolNotEnforcingDeps(), not
        // DescriptorPool::FindEnumValueByName() because we're already holding
        // the pool's mutex, and the latter method locks it again.
        Symbol symbol =
            builder_->FindSymbolNotEnforcingDeps(fully_qualified_name);
        if (!symbol.IsNull() && symbol.type == Symbol::ENUM_VALUE) {
          if (symbol.enum_value_descriptor->type() != enum_type) {
            return AddValueError(
                "Enum type \"" + enum_type->full_name() +
                "\" has no value named \"" + value_name + "\" for option \"" +
                option_field->full_name() +
                "\". This appears to be a value from a sibling type.");
          } else {
            enum_value = symbol.enum_value_descriptor;
          }
        }
      } else {
        // The enum type is in the generated pool, so we can search for the
        // value there.
        enum_value = enum_type->FindValueByName(value_name);
      }

      if (enum_value == nullptr) {
        return AddValueError("Enum type \"" +
                             option_field->enum_type()->full_name() +
                             "\" has no value named \"" + value_name +
                             "\" for "
                             "option \"" +
                             option_field->full_name() + "\".");
      } else {
        // Sign-extension is not a problem, since we cast directly from int32 to
        // uint64, without first going through uint32.
        unknown_fields->AddVarint(
            option_field->number(),
            static_cast<uint64>(static_cast<int64>(enum_value->number())));
      }
      break;
    }

    case FieldDescriptor::CPPTYPE_STRING:
      if (!uninterpreted_option_->has_string_value()) {
        return AddValueError(
            "Value must be quoted string for string option "
            "\"" +
            option_field->full_name() + "\".");
      }
      // The string has already been unquoted and unescaped by the parser.
      unknown_fields->AddLengthDelimited(option_field->number(),
                                         uninterpreted_option_->string_value());
      break;

    case FieldDescriptor::CPPTYPE_MESSAGE:
      if (!SetAggregateOption(option_field, unknown_fields)) {
        return false;
      }
      break;
  }

  return true;
}

class DescriptorBuilder::OptionInterpreter::AggregateOptionFinder
    : public TextFormat::Finder {
 public:
  DescriptorBuilder* builder_;

  const FieldDescriptor* FindExtension(Message* message,
                                       const std::string& name) const override {
    assert_mutex_held(builder_->pool_);
    const Descriptor* descriptor = message->GetDescriptor();
    Symbol result =
        builder_->LookupSymbolNoPlaceholder(name, descriptor->full_name());
    if (result.type == Symbol::FIELD &&
        result.field_descriptor->is_extension()) {
      return result.field_descriptor;
    } else if (result.type == Symbol::MESSAGE &&
               descriptor->options().message_set_wire_format()) {
      const Descriptor* foreign_type = result.descriptor;
      // The text format allows MessageSet items to be specified using
      // the type name, rather than the extension identifier. If the symbol
      // lookup returned a Message, and the enclosing Message has
      // message_set_wire_format = true, then return the message set
      // extension, if one exists.
      for (int i = 0; i < foreign_type->extension_count(); i++) {
        const FieldDescriptor* extension = foreign_type->extension(i);
        if (extension->containing_type() == descriptor &&
            extension->type() == FieldDescriptor::TYPE_MESSAGE &&
            extension->is_optional() &&
            extension->message_type() == foreign_type) {
          // Found it.
          return extension;
        }
      }
    }
    return nullptr;
  }
};

// A custom error collector to record any text-format parsing errors
namespace {
class AggregateErrorCollector : public io::ErrorCollector {
 public:
  std::string error_;

  void AddError(int /* line */, int /* column */,
                const std::string& message) override {
    if (!error_.empty()) {
      error_ += "; ";
    }
    error_ += message;
  }

  void AddWarning(int /* line */, int /* column */,
                  const std::string& /* message */) override {
    // Ignore warnings
  }
};
}  // namespace

// We construct a dynamic message of the type corresponding to
// option_field, parse the supplied text-format string into this
// message, and serialize the resulting message to produce the value.
bool DescriptorBuilder::OptionInterpreter::SetAggregateOption(
    const FieldDescriptor* option_field, UnknownFieldSet* unknown_fields) {
  if (!uninterpreted_option_->has_aggregate_value()) {
    return AddValueError("Option \"" + option_field->full_name() +
                         "\" is a message. To set the entire message, use "
                         "syntax like \"" +
                         option_field->name() +
                         " = { <proto text format> }\". "
                         "To set fields within it, use "
                         "syntax like \"" +
                         option_field->name() + ".foo = value\".");
  }

  const Descriptor* type = option_field->message_type();
  std::unique_ptr<Message> dynamic(dynamic_factory_.GetPrototype(type)->New());
  GOOGLE_CHECK(dynamic.get() != nullptr)
      << "Could not create an instance of " << option_field->DebugString();

  AggregateErrorCollector collector;
  AggregateOptionFinder finder;
  finder.builder_ = builder_;
  TextFormat::Parser parser;
  parser.RecordErrorsTo(&collector);
  parser.SetFinder(&finder);
  if (!parser.ParseFromString(uninterpreted_option_->aggregate_value(),
                              dynamic.get())) {
    AddValueError("Error while parsing option value for \"" +
                  option_field->name() + "\": " + collector.error_);
    return false;
  } else {
    std::string serial;
    dynamic->SerializeToString(&serial);  // Never fails
    if (option_field->type() == FieldDescriptor::TYPE_MESSAGE) {
      unknown_fields->AddLengthDelimited(option_field->number(), serial);
    } else {
      GOOGLE_CHECK_EQ(option_field->type(), FieldDescriptor::TYPE_GROUP);
      UnknownFieldSet* group = unknown_fields->AddGroup(option_field->number());
      group->ParseFromString(serial);
    }
    return true;
  }
}

void DescriptorBuilder::OptionInterpreter::SetInt32(
    int number, int32 value, FieldDescriptor::Type type,
    UnknownFieldSet* unknown_fields) {
  switch (type) {
    case FieldDescriptor::TYPE_INT32:
      unknown_fields->AddVarint(number,
                                static_cast<uint64>(static_cast<int64>(value)));
      break;

    case FieldDescriptor::TYPE_SFIXED32:
      unknown_fields->AddFixed32(number, static_cast<uint32>(value));
      break;

    case FieldDescriptor::TYPE_SINT32:
      unknown_fields->AddVarint(
          number, internal::WireFormatLite::ZigZagEncode32(value));
      break;

    default:
      GOOGLE_LOG(FATAL) << "Invalid wire type for CPPTYPE_INT32: " << type;
      break;
  }
}

void DescriptorBuilder::OptionInterpreter::SetInt64(
    int number, int64 value, FieldDescriptor::Type type,
    UnknownFieldSet* unknown_fields) {
  switch (type) {
    case FieldDescriptor::TYPE_INT64:
      unknown_fields->AddVarint(number, static_cast<uint64>(value));
      break;

    case FieldDescriptor::TYPE_SFIXED64:
      unknown_fields->AddFixed64(number, static_cast<uint64>(value));
      break;

    case FieldDescriptor::TYPE_SINT64:
      unknown_fields->AddVarint(
          number, internal::WireFormatLite::ZigZagEncode64(value));
      break;

    default:
      GOOGLE_LOG(FATAL) << "Invalid wire type for CPPTYPE_INT64: " << type;
      break;
  }
}

void DescriptorBuilder::OptionInterpreter::SetUInt32(
    int number, uint32 value, FieldDescriptor::Type type,
    UnknownFieldSet* unknown_fields) {
  switch (type) {
    case FieldDescriptor::TYPE_UINT32:
      unknown_fields->AddVarint(number, static_cast<uint64>(value));
      break;

    case FieldDescriptor::TYPE_FIXED32:
      unknown_fields->AddFixed32(number, static_cast<uint32>(value));
      break;

    default:
      GOOGLE_LOG(FATAL) << "Invalid wire type for CPPTYPE_UINT32: " << type;
      break;
  }
}

void DescriptorBuilder::OptionInterpreter::SetUInt64(
    int number, uint64 value, FieldDescriptor::Type type,
    UnknownFieldSet* unknown_fields) {
  switch (type) {
    case FieldDescriptor::TYPE_UINT64:
      unknown_fields->AddVarint(number, value);
      break;

    case FieldDescriptor::TYPE_FIXED64:
      unknown_fields->AddFixed64(number, value);
      break;

    default:
      GOOGLE_LOG(FATAL) << "Invalid wire type for CPPTYPE_UINT64: " << type;
      break;
  }
}

void DescriptorBuilder::LogUnusedDependency(const FileDescriptorProto& proto,
                                            const FileDescriptor* result) {

  if (!unused_dependency_.empty()) {
    std::set<std::string> annotation_extensions;
    annotation_extensions.insert("google.protobuf.MessageOptions");
    annotation_extensions.insert("google.protobuf.FileOptions");
    annotation_extensions.insert("google.protobuf.FieldOptions");
    annotation_extensions.insert("google.protobuf.EnumOptions");
    annotation_extensions.insert("google.protobuf.EnumValueOptions");
    annotation_extensions.insert("google.protobuf.EnumValueOptions");
    annotation_extensions.insert("google.protobuf.ServiceOptions");
    annotation_extensions.insert("google.protobuf.MethodOptions");
    annotation_extensions.insert("google.protobuf.StreamOptions");
    for (std::set<const FileDescriptor*>::const_iterator it =
             unused_dependency_.begin();
         it != unused_dependency_.end(); ++it) {
      // Do not log warnings for proto files which extend annotations.
      int i;
      for (i = 0; i < (*it)->extension_count(); ++i) {
        if (annotation_extensions.find(
                (*it)->extension(i)->containing_type()->full_name()) !=
            annotation_extensions.end()) {
          break;
        }
      }
      // Log warnings for unused imported files.
      if (i == (*it)->extension_count()) {
        std::string error_message =
            "Import " + (*it)->name() + " but not used.";
        AddWarning((*it)->name(), proto, DescriptorPool::ErrorCollector::IMPORT,
                   error_message);
      }
    }
  }
}

Symbol DescriptorPool::CrossLinkOnDemandHelper(const std::string& name,
                                               bool expecting_enum) const {
  std::string lookup_name = name;
  if (!lookup_name.empty() && lookup_name[0] == '.') {
    lookup_name = lookup_name.substr(1);
  }
  Symbol result = tables_->FindByNameHelper(this, lookup_name);
  return result;
}

// Handle the lazy import building for a message field whose type wasn't built
// at cross link time. If that was the case, we saved the name of the type to
// be looked up when the accessor for the type was called. Set type_,
// enum_type_, message_type_, and default_value_enum_ appropriately.
void FieldDescriptor::InternalTypeOnceInit() const {
  GOOGLE_CHECK(file()->finished_building_ == true);
  if (type_name_) {
    Symbol result = file()->pool()->CrossLinkOnDemandHelper(
        *type_name_, type_ == FieldDescriptor::TYPE_ENUM);
    if (result.type == Symbol::MESSAGE) {
      type_ = FieldDescriptor::TYPE_MESSAGE;
      message_type_ = result.descriptor;
    } else if (result.type == Symbol::ENUM) {
      type_ = FieldDescriptor::TYPE_ENUM;
      enum_type_ = result.enum_descriptor;
    }
  }
  if (enum_type_ && !default_value_enum_) {
    if (default_value_enum_name_) {
      // Have to build the full name now instead of at CrossLink time,
      // because enum_type_ may not be known at the time.
      std::string name = enum_type_->full_name();
      // Enum values reside in the same scope as the enum type.
      std::string::size_type last_dot = name.find_last_of('.');
      if (last_dot != std::string::npos) {
        name = name.substr(0, last_dot) + "." + *default_value_enum_name_;
      } else {
        name = *default_value_enum_name_;
      }
      Symbol result = file()->pool()->CrossLinkOnDemandHelper(name, true);
      if (result.type == Symbol::ENUM_VALUE) {
        default_value_enum_ = result.enum_value_descriptor;
      }
    }
    if (!default_value_enum_) {
      // We use the first defined value as the default
      // if a default is not explicitly defined.
      GOOGLE_CHECK(enum_type_->value_count());
      default_value_enum_ = enum_type_->value(0);
    }
  }
}

void FieldDescriptor::TypeOnceInit(const FieldDescriptor* to_init) {
  to_init->InternalTypeOnceInit();
}

// message_type(), enum_type(), default_value_enum(), and type()
// all share the same internal::call_once init path to do lazy
// import building and cross linking of a field of a message.
const Descriptor* FieldDescriptor::message_type() const {
  if (type_once_) {
    internal::call_once(*type_once_, FieldDescriptor::TypeOnceInit, this);
  }
  return message_type_;
}

const EnumDescriptor* FieldDescriptor::enum_type() const {
  if (type_once_) {
    internal::call_once(*type_once_, FieldDescriptor::TypeOnceInit, this);
  }
  return enum_type_;
}

const EnumValueDescriptor* FieldDescriptor::default_value_enum() const {
  if (type_once_) {
    internal::call_once(*type_once_, FieldDescriptor::TypeOnceInit, this);
  }
  return default_value_enum_;
}

const std::string& FieldDescriptor::PrintableNameForExtension() const {
  const bool is_message_set_extension =
      is_extension() &&
      containing_type()->options().message_set_wire_format() &&
      type() == FieldDescriptor::TYPE_MESSAGE && is_optional() &&
      extension_scope() == message_type();
  return is_message_set_extension ? message_type()->full_name() : full_name();
}

void FileDescriptor::InternalDependenciesOnceInit() const {
  GOOGLE_CHECK(finished_building_ == true);
  for (int i = 0; i < dependency_count(); i++) {
    if (dependencies_names_[i]) {
      dependencies_[i] = pool_->FindFileByName(*dependencies_names_[i]);
    }
  }
}

void FileDescriptor::DependenciesOnceInit(const FileDescriptor* to_init) {
  to_init->InternalDependenciesOnceInit();
}

const FileDescriptor* FileDescriptor::dependency(int index) const {
  if (dependencies_once_) {
    // Do once init for all indices, as it's unlikely only a single index would
    // be called, and saves on internal::call_once allocations.
    internal::call_once(*dependencies_once_,
                        FileDescriptor::DependenciesOnceInit, this);
  }
  return dependencies_[index];
}

const Descriptor* MethodDescriptor::input_type() const {
  return input_type_.Get();
}

const Descriptor* MethodDescriptor::output_type() const {
  return output_type_.Get();
}


namespace internal {
void LazyDescriptor::Set(const Descriptor* descriptor) {
  GOOGLE_CHECK(!name_);
  GOOGLE_CHECK(!once_);
  GOOGLE_CHECK(!file_);
  descriptor_ = descriptor;
}

void LazyDescriptor::SetLazy(const std::string& name,
                             const FileDescriptor* file) {
  // verify Init() has been called and Set hasn't been called yet.
  GOOGLE_CHECK(!descriptor_);
  GOOGLE_CHECK(!file_);
  GOOGLE_CHECK(!name_);
  GOOGLE_CHECK(!once_);
  GOOGLE_CHECK(file && file->pool_);
  GOOGLE_CHECK(file->pool_->lazily_build_dependencies_);
  GOOGLE_CHECK(!file->finished_building_);
  file_ = file;
  name_ = file->pool_->tables_->AllocateString(name);
  once_ = file->pool_->tables_->AllocateOnceDynamic();
}

void LazyDescriptor::Once() {
  if (once_) {
    internal::call_once(*once_, LazyDescriptor::OnceStatic, this);
  }
}

void LazyDescriptor::OnceStatic(LazyDescriptor* lazy) { lazy->OnceInternal(); }

void LazyDescriptor::OnceInternal() {
  GOOGLE_CHECK(file_->finished_building_);
  if (!descriptor_ && name_) {
    Symbol result = file_->pool_->CrossLinkOnDemandHelper(*name_, false);
    if (!result.IsNull() && result.type == Symbol::MESSAGE) {
      descriptor_ = result.descriptor;
    }
  }
}
}  // namespace internal

}  // namespace protobuf
}  // namespace google
