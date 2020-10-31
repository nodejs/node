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

#ifndef GOOGLE_PROTOBUF_COMPILER_CPP_HELPERS_H__
#define GOOGLE_PROTOBUF_COMPILER_CPP_HELPERS_H__

#include <algorithm>
#include <iterator>
#include <map>
#include <string>

#include <google/protobuf/compiler/cpp/cpp_options.h>
#include <google/protobuf/compiler/scc.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/port.h>
#include <google/protobuf/stubs/strutil.h>


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

inline std::string ProtobufNamespace(const Options& options) {
  return "PROTOBUF_NAMESPACE_ID";
}

inline std::string MacroPrefix(const Options& options) {
  return options.opensource_runtime ? "GOOGLE_PROTOBUF" : "GOOGLE_PROTOBUF";
}

inline std::string DeprecatedAttribute(const Options& options,
                                       bool deprecated) {
  return deprecated ? "PROTOBUF_DEPRECATED " : "";
}

// Commonly-used separator comments.  Thick is a line of '=', thin is a line
// of '-'.
extern const char kThickSeparator[];
extern const char kThinSeparator[];

void SetCommonVars(const Options& options,
                   std::map<std::string, std::string>* variables);

bool GetBootstrapBasename(const Options& options, const std::string& basename,
                          std::string* bootstrap_basename);
bool MaybeBootstrap(const Options& options, GeneratorContext* generator_context,
                    bool bootstrap_flag, std::string* basename);
bool IsBootstrapProto(const Options& options, const FileDescriptor* file);

// Name space of the proto file. This namespace is such that the string
// "<namespace>::some_name" is the correct fully qualified namespace.
// This means if the package is empty the namespace is "", and otherwise
// the namespace is "::foo::bar::...::baz" without trailing semi-colons.
std::string Namespace(const FileDescriptor* d, const Options& options);
std::string Namespace(const Descriptor* d, const Options& options);
std::string Namespace(const FieldDescriptor* d, const Options& options);
std::string Namespace(const EnumDescriptor* d, const Options& options);

// Returns true if it's safe to reset "field" to zero.
bool CanInitializeByZeroing(const FieldDescriptor* field);

std::string ClassName(const Descriptor* descriptor);
std::string ClassName(const EnumDescriptor* enum_descriptor);

std::string QualifiedClassName(const Descriptor* d, const Options& options);
std::string QualifiedClassName(const EnumDescriptor* d, const Options& options);

std::string QualifiedClassName(const Descriptor* d);
std::string QualifiedClassName(const EnumDescriptor* d);

// DEPRECATED just use ClassName or QualifiedClassName, a boolean is very
// unreadable at the callsite.
// Returns the non-nested type name for the given type.  If "qualified" is
// true, prefix the type with the full namespace.  For example, if you had:
//   package foo.bar;
//   message Baz { message Qux {} }
// Then the qualified ClassName for Qux would be:
//   ::foo::bar::Baz_Qux
// While the non-qualified version would be:
//   Baz_Qux
inline std::string ClassName(const Descriptor* descriptor, bool qualified) {
  return qualified ? QualifiedClassName(descriptor, Options())
                   : ClassName(descriptor);
}

inline std::string ClassName(const EnumDescriptor* descriptor, bool qualified) {
  return qualified ? QualifiedClassName(descriptor, Options())
                   : ClassName(descriptor);
}

// Type name of default instance.
std::string DefaultInstanceType(const Descriptor* descriptor,
                                const Options& options);

// Non-qualified name of the default_instance of this message.
std::string DefaultInstanceName(const Descriptor* descriptor,
                                const Options& options);

// Fully qualified name of the default_instance of this message.
std::string QualifiedDefaultInstanceName(const Descriptor* descriptor,
                                         const Options& options);

// DescriptorTable variable name.
std::string DescriptorTableName(const FileDescriptor* file,
                                const Options& options);

// When declaring symbol externs from another file, this macro will supply the
// dllexport needed for the target file, if any.
std::string FileDllExport(const FileDescriptor* file, const Options& options);

// Returns the name of a no-op function that we can call to introduce a linker
// dependency on the given message type. This is used to implement implicit weak
// fields.
std::string ReferenceFunctionName(const Descriptor* descriptor,
                                  const Options& options);

// Name of the base class: google::protobuf::Message or google::protobuf::MessageLite.
std::string SuperClassName(const Descriptor* descriptor,
                           const Options& options);

// Adds an underscore if necessary to prevent conflicting with a keyword.
std::string ResolveKeyword(const string& name);

// Get the (unqualified) name that should be used for this field in C++ code.
// The name is coerced to lower-case to emulate proto1 behavior.  People
// should be using lowercase-with-underscores style for proto field names
// anyway, so normally this just returns field->name().
std::string FieldName(const FieldDescriptor* field);

// Get the sanitized name that should be used for the given enum in C++ code.
std::string EnumValueName(const EnumValueDescriptor* enum_value);

// Returns an estimate of the compiler's alignment for the field.  This
// can't guarantee to be correct because the generated code could be compiled on
// different systems with different alignment rules.  The estimates below assume
// 64-bit pointers.
int EstimateAlignmentSize(const FieldDescriptor* field);

// Get the unqualified name that should be used for a field's field
// number constant.
std::string FieldConstantName(const FieldDescriptor* field);

// Returns the scope where the field was defined (for extensions, this is
// different from the message type to which the field applies).
inline const Descriptor* FieldScope(const FieldDescriptor* field) {
  return field->is_extension() ? field->extension_scope()
                               : field->containing_type();
}

// Returns the fully-qualified type name field->message_type().  Usually this
// is just ClassName(field->message_type(), true);
std::string FieldMessageTypeName(const FieldDescriptor* field,
                                 const Options& options);

// Strips ".proto" or ".protodevel" from the end of a filename.
PROTOC_EXPORT std::string StripProto(const std::string& filename);

// Get the C++ type name for a primitive type (e.g. "double", "::google::protobuf::int32", etc.).
const char* PrimitiveTypeName(FieldDescriptor::CppType type);
std::string PrimitiveTypeName(const Options& options,
                              FieldDescriptor::CppType type);

// Get the declared type name in CamelCase format, as is used e.g. for the
// methods of WireFormat.  For example, TYPE_INT32 becomes "Int32".
const char* DeclaredTypeMethodName(FieldDescriptor::Type type);

// Return the code that evaluates to the number when compiled.
std::string Int32ToString(int number);

// Return the code that evaluates to the number when compiled.
std::string Int64ToString(const Options& options, int64 number);

// Get code that evaluates to the field's default value.
std::string DefaultValue(const Options& options, const FieldDescriptor* field);

// Compatibility function for callers outside proto2.
std::string DefaultValue(const FieldDescriptor* field);

// Convert a file name into a valid identifier.
std::string FilenameIdentifier(const std::string& filename);

// For each .proto file generates a unique name. To prevent collisions of
// symbols in the global namespace
std::string UniqueName(const std::string& name, const std::string& filename,
                       const Options& options);
inline std::string UniqueName(const std::string& name, const FileDescriptor* d,
                              const Options& options) {
  return UniqueName(name, d->name(), options);
}
inline std::string UniqueName(const std::string& name, const Descriptor* d,
                              const Options& options) {
  return UniqueName(name, d->file(), options);
}
inline std::string UniqueName(const std::string& name, const EnumDescriptor* d,
                              const Options& options) {
  return UniqueName(name, d->file(), options);
}
inline std::string UniqueName(const std::string& name,
                              const ServiceDescriptor* d,
                              const Options& options) {
  return UniqueName(name, d->file(), options);
}

// Versions for call sites that only support the internal runtime (like proto1
// support).
inline Options InternalRuntimeOptions() {
  Options options;
  options.opensource_runtime = false;
  return options;
}
inline std::string UniqueName(const std::string& name,
                              const std::string& filename) {
  return UniqueName(name, filename, InternalRuntimeOptions());
}
inline std::string UniqueName(const std::string& name,
                              const FileDescriptor* d) {
  return UniqueName(name, d->name(), InternalRuntimeOptions());
}
inline std::string UniqueName(const std::string& name, const Descriptor* d) {
  return UniqueName(name, d->file(), InternalRuntimeOptions());
}
inline std::string UniqueName(const std::string& name,
                              const EnumDescriptor* d) {
  return UniqueName(name, d->file(), InternalRuntimeOptions());
}
inline std::string UniqueName(const std::string& name,
                              const ServiceDescriptor* d) {
  return UniqueName(name, d->file(), InternalRuntimeOptions());
}

// Return the qualified C++ name for a file level symbol.
std::string QualifiedFileLevelSymbol(const FileDescriptor* file,
                                     const std::string& name,
                                     const Options& options);

// Escape C++ trigraphs by escaping question marks to \?
std::string EscapeTrigraphs(const std::string& to_escape);

// Escaped function name to eliminate naming conflict.
std::string SafeFunctionName(const Descriptor* descriptor,
                             const FieldDescriptor* field,
                             const std::string& prefix);

// Returns true if generated messages have public unknown fields accessors
inline bool PublicUnknownFieldsAccessors(const Descriptor* message) {
  return message->file()->syntax() != FileDescriptor::SYNTAX_PROTO3;
}

// Returns the optimize mode for <file>, respecting <options.enforce_lite>.
FileOptions_OptimizeMode GetOptimizeFor(const FileDescriptor* file,
                                        const Options& options);

// Determines whether unknown fields will be stored in an UnknownFieldSet or
// a string.
inline bool UseUnknownFieldSet(const FileDescriptor* file,
                               const Options& options) {
  return GetOptimizeFor(file, options) != FileOptions::LITE_RUNTIME;
}

inline bool IsWeak(const FieldDescriptor* field, const Options& options) {
  if (field->options().weak()) {
    GOOGLE_CHECK(!options.opensource_runtime);
    return true;
  }
  return false;
}

bool IsStringInlined(const FieldDescriptor* descriptor, const Options& options);

// For a string field, returns the effective ctype.  If the actual ctype is
// not supported, returns the default of STRING.
FieldOptions::CType EffectiveStringCType(const FieldDescriptor* field,
                                         const Options& options);

inline bool IsCord(const FieldDescriptor* field, const Options& options) {
  return field->cpp_type() == FieldDescriptor::CPPTYPE_STRING &&
         EffectiveStringCType(field, options) == FieldOptions::CORD;
}

inline bool IsStringPiece(const FieldDescriptor* field,
                          const Options& options) {
  return field->cpp_type() == FieldDescriptor::CPPTYPE_STRING &&
         EffectiveStringCType(field, options) == FieldOptions::STRING_PIECE;
}

// Does the given FileDescriptor use lazy fields?
bool HasLazyFields(const FileDescriptor* file, const Options& options);

// Is the given field a supported lazy field?
inline bool IsLazy(const FieldDescriptor* field, const Options& options) {
  return field->options().lazy() && !field->is_repeated() &&
         field->type() == FieldDescriptor::TYPE_MESSAGE &&
         GetOptimizeFor(field->file(), options) != FileOptions::LITE_RUNTIME &&
         !options.opensource_runtime;
}

// Does the file contain any definitions that need extension_set.h?
bool HasExtensionsOrExtendableMessage(const FileDescriptor* file);

// Does the file have any repeated fields, necessitating the file to include
// repeated_field.h? This does not include repeated extensions, since those are
// all stored internally in an ExtensionSet, not a separate RepeatedField*.
bool HasRepeatedFields(const FileDescriptor* file);

// Does the file have any string/bytes fields with ctype=STRING_PIECE? This
// does not include extensions, since ctype is ignored for extensions.
bool HasStringPieceFields(const FileDescriptor* file, const Options& options);

// Does the file have any string/bytes fields with ctype=CORD? This does not
// include extensions, since ctype is ignored for extensions.
bool HasCordFields(const FileDescriptor* file, const Options& options);

// Does the file have any map fields, necessitating the file to include
// map_field_inl.h and map.h.
bool HasMapFields(const FileDescriptor* file);

// Does this file have any enum type definitions?
bool HasEnumDefinitions(const FileDescriptor* file);

// Does this file have generated parsing, serialization, and other
// standard methods for which reflection-based fallback implementations exist?
inline bool HasGeneratedMethods(const FileDescriptor* file,
                                const Options& options) {
  return GetOptimizeFor(file, options) != FileOptions::CODE_SIZE;
}

// Do message classes in this file have descriptor and reflection methods?
inline bool HasDescriptorMethods(const FileDescriptor* file,
                                 const Options& options) {
  return GetOptimizeFor(file, options) != FileOptions::LITE_RUNTIME;
}

// Should we generate generic services for this file?
inline bool HasGenericServices(const FileDescriptor* file,
                               const Options& options) {
  return file->service_count() > 0 &&
         GetOptimizeFor(file, options) != FileOptions::LITE_RUNTIME &&
         file->options().cc_generic_services();
}

// Should we generate a separate, super-optimized code path for serializing to
// flat arrays?  We don't do this in Lite mode because we'd rather reduce code
// size.
inline bool HasFastArraySerialization(const FileDescriptor* file,
                                      const Options& options) {
  return GetOptimizeFor(file, options) == FileOptions::SPEED;
}

inline bool IsProto2MessageSet(const Descriptor* descriptor,
                               const Options& options) {
  return !options.opensource_runtime &&
         options.enforce_mode != EnforceOptimizeMode::kLiteRuntime &&
         !options.lite_implicit_weak_fields &&
         descriptor->options().message_set_wire_format() &&
         descriptor->full_name() == "google.protobuf.bridge.MessageSet";
}

inline bool IsProto2MessageSetFile(const FileDescriptor* file,
                                   const Options& options) {
  return !options.opensource_runtime &&
         options.enforce_mode != EnforceOptimizeMode::kLiteRuntime &&
         !options.lite_implicit_weak_fields &&
         file->name() == "net/proto2/bridge/proto/message_set.proto";
}

inline bool IsMapEntryMessage(const Descriptor* descriptor) {
  return descriptor->options().map_entry();
}

// Returns true if the field's CPPTYPE is string or message.
bool IsStringOrMessage(const FieldDescriptor* field);

std::string UnderscoresToCamelCase(const std::string& input,
                                   bool cap_next_letter);

inline bool HasFieldPresence(const FileDescriptor* file) {
  return file->syntax() != FileDescriptor::SYNTAX_PROTO3;
}

// Returns true if 'enum' semantics are such that unknown values are preserved
// in the enum field itself, rather than going to the UnknownFieldSet.
inline bool HasPreservingUnknownEnumSemantics(const FieldDescriptor* field) {
  return field->file()->syntax() == FileDescriptor::SYNTAX_PROTO3;
}

inline bool SupportsArenas(const FileDescriptor* file) {
  return file->options().cc_enable_arenas();
}

inline bool SupportsArenas(const Descriptor* desc) {
  return SupportsArenas(desc->file());
}

inline bool SupportsArenas(const FieldDescriptor* field) {
  return SupportsArenas(field->file());
}

inline bool IsCrossFileMessage(const FieldDescriptor* field) {
  return field->type() == FieldDescriptor::TYPE_MESSAGE &&
         field->message_type()->file() != field->file();
}

inline std::string MessageCreateFunction(const Descriptor* d) {
  return SupportsArenas(d) ? "CreateMessage" : "Create";
}

inline std::string MakeDefaultName(const FieldDescriptor* field) {
  return "_i_give_permission_to_break_this_code_default_" + FieldName(field) +
         "_";
}

bool IsAnyMessage(const FileDescriptor* descriptor, const Options& options);
bool IsAnyMessage(const Descriptor* descriptor, const Options& options);

bool IsWellKnownMessage(const FileDescriptor* descriptor);

inline std::string IncludeGuard(const FileDescriptor* file, bool pb_h,
                                const Options& options) {
  // If we are generating a .pb.h file and the proto_h option is enabled, then
  // the .pb.h gets an extra suffix.
  std::string filename_identifier = FilenameIdentifier(
      file->name() + (pb_h && options.proto_h ? ".pb.h" : ""));

  if (IsWellKnownMessage(file)) {
    // For well-known messages we need third_party/protobuf and net/proto2 to
    // have distinct include guards, because some source files include both and
    // both need to be defined (the third_party copies will be in the
    // google::protobuf_opensource namespace).
    return MacroPrefix(options) + "_INCLUDED_" + filename_identifier;
  } else {
    // Ideally this case would use distinct include guards for opensource and
    // google3 protos also.  (The behavior of "first #included wins" is not
    // ideal).  But unfortunately some legacy code includes both and depends on
    // the identical include guards to avoid compile errors.
    //
    // We should clean this up so that this case can be removed.
    return "GOOGLE_PROTOBUF_INCLUDED_" + filename_identifier;
  }
}

inline FileOptions_OptimizeMode GetOptimizeFor(const FileDescriptor* file,
                                               const Options& options) {
  switch (options.enforce_mode) {
    case EnforceOptimizeMode::kSpeed:
      return FileOptions::SPEED;
    case EnforceOptimizeMode::kLiteRuntime:
      return FileOptions::LITE_RUNTIME;
    case EnforceOptimizeMode::kNoEnforcement:
    default:
      return file->options().optimize_for();
  }
}

// This orders the messages in a .pb.cc as it's outputted by file.cc
void FlattenMessagesInFile(const FileDescriptor* file,
                           std::vector<const Descriptor*>* result);
inline std::vector<const Descriptor*> FlattenMessagesInFile(
    const FileDescriptor* file) {
  std::vector<const Descriptor*> result;
  FlattenMessagesInFile(file, &result);
  return result;
}

bool HasWeakFields(const Descriptor* desc, const Options& options);
bool HasWeakFields(const FileDescriptor* desc, const Options& options);

// Returns true if the "required" restriction check should be ignored for the
// given field.
inline static bool ShouldIgnoreRequiredFieldCheck(const FieldDescriptor* field,
                                                  const Options& options) {
  // Do not check "required" for lazy fields.
  return IsLazy(field, options);
}

struct MessageAnalysis {
  bool is_recursive;
  bool contains_cord;
  bool contains_extension;
  bool contains_required;
  bool constructor_requires_initialization;
};

// This class is used in FileGenerator, to ensure linear instead of
// quadratic performance, if we do this per message we would get O(V*(V+E)).
// Logically this is just only used in message.cc, but in the header for
// FileGenerator to help share it.
class PROTOC_EXPORT MessageSCCAnalyzer {
 public:
  explicit MessageSCCAnalyzer(const Options& options) : options_(options) {}

  MessageAnalysis GetSCCAnalysis(const SCC* scc);

  bool HasRequiredFields(const Descriptor* descriptor) {
    MessageAnalysis result = GetSCCAnalysis(GetSCC(descriptor));
    return result.contains_required || result.contains_extension;
  }
  const SCC* GetSCC(const Descriptor* descriptor) {
    return analyzer_.GetSCC(descriptor);
  }

 private:
  struct DepsGenerator {
    std::vector<const Descriptor*> operator()(const Descriptor* desc) const {
      std::vector<const Descriptor*> deps;
      for (int i = 0; i < desc->field_count(); i++) {
        if (desc->field(i)->message_type()) {
          deps.push_back(desc->field(i)->message_type());
        }
      }
      return deps;
    }
  };
  SCCAnalyzer<DepsGenerator> analyzer_;
  Options options_;
  std::map<const SCC*, MessageAnalysis> analysis_cache_;
};

inline std::string SccInfoSymbol(const SCC* scc, const Options& options) {
  return UniqueName("scc_info_" + ClassName(scc->GetRepresentative()),
                    scc->GetRepresentative(), options);
}

void ListAllFields(const Descriptor* d,
                   std::vector<const FieldDescriptor*>* fields);
void ListAllFields(const FileDescriptor* d,
                   std::vector<const FieldDescriptor*>* fields);

template <class T>
void ForEachField(const Descriptor* d, T&& func) {
  for (int i = 0; i < d->nested_type_count(); i++) {
    ForEachField(d->nested_type(i), std::forward<T&&>(func));
  }
  for (int i = 0; i < d->extension_count(); i++) {
    func(d->extension(i));
  }
  for (int i = 0; i < d->field_count(); i++) {
    func(d->field(i));
  }
}

template <class T>
void ForEachField(const FileDescriptor* d, T&& func) {
  for (int i = 0; i < d->message_type_count(); i++) {
    ForEachField(d->message_type(i), std::forward<T&&>(func));
  }
  for (int i = 0; i < d->extension_count(); i++) {
    func(d->extension(i));
  }
}

void ListAllTypesForServices(const FileDescriptor* fd,
                             std::vector<const Descriptor*>* types);

// Indicates whether we should use implicit weak fields for this file.
bool UsingImplicitWeakFields(const FileDescriptor* file,
                             const Options& options);

// Indicates whether to treat this field as implicitly weak.
bool IsImplicitWeakField(const FieldDescriptor* field, const Options& options,
                         MessageSCCAnalyzer* scc_analyzer);

// Formatter is a functor class which acts as a closure around printer and
// the variable map. It's much like printer->Print except it supports both named
// variables that are substituted using a key value map and direct arguments. In
// the format string $1$, $2$, etc... are substituted for the first, second, ...
// direct argument respectively in the format call, it accepts both strings and
// integers. The implementation verifies all arguments are used and are "first"
// used in order of appearance in the argument list. For example,
//
// Format("return array[$1$];", 3) -> "return array[3];"
// Format("array[$2$] = $1$;", "Bla", 3) -> FATAL error (wrong order)
// Format("array[$1$] = $2$;", 3, "Bla") -> "array[3] = Bla;"
//
// The arguments can be used more than once like
//
// Format("array[$1$] = $2$;  // Index = $1$", 3, "Bla") ->
//        "array[3] = Bla;  // Index = 3"
//
// If you use more arguments use the following style to help the reader,
//
// Format("int $1$() {\n"
//        "  array[$2$] = $3$;\n"
//        "  return $4$;"
//        "}\n",
//        funname, // 1
//        idx,  // 2
//        varname,  // 3
//        retval);  // 4
//
// but consider using named variables. Named variables like $foo$, with some
// identifier foo, are looked up in the map. One additional feature is that
// spaces are accepted between the '$' delimiters, $ foo$ will
// substiture to " bar" if foo stands for "bar", but in case it's empty
// will substitute to "". Hence, for example,
//
// Format(vars, "$dllexport $void fun();") -> "void fun();"
//                                            "__declspec(export) void fun();"
//
// which is convenient to prevent double, leading or trailing spaces.
class PROTOC_EXPORT Formatter {
 public:
  explicit Formatter(io::Printer* printer) : printer_(printer) {}
  Formatter(io::Printer* printer,
            const std::map<std::string, std::string>& vars)
      : printer_(printer), vars_(vars) {}

  template <typename T>
  void Set(const std::string& key, const T& value) {
    vars_[key] = ToString(value);
  }

  void AddMap(const std::map<std::string, std::string>& vars) {
    for (const auto& keyval : vars) vars_[keyval.first] = keyval.second;
  }

  template <typename... Args>
  void operator()(const char* format, const Args&... args) const {
    printer_->FormatInternal({ToString(args)...}, vars_, format);
  }

  void Indent() const { printer_->Indent(); }
  void Outdent() const { printer_->Outdent(); }
  io::Printer* printer() const { return printer_; }

  class PROTOC_EXPORT SaveState {
   public:
    explicit SaveState(Formatter* format)
        : format_(format), vars_(format->vars_) {}
    ~SaveState() { format_->vars_.swap(vars_); }

   private:
    Formatter* format_;
    std::map<std::string, std::string> vars_;
  };

 private:
  io::Printer* printer_;
  std::map<std::string, std::string> vars_;

  // Convenience overloads to accept different types as arguments.
  static std::string ToString(const std::string& s) { return s; }
  template <typename I, typename = typename std::enable_if<
                            std::is_integral<I>::value>::type>
  static std::string ToString(I x) {
    return StrCat(x);
  }
  static std::string ToString(strings::Hex x) { return StrCat(x); }
  static std::string ToString(const FieldDescriptor* d) { return Payload(d); }
  static std::string ToString(const Descriptor* d) { return Payload(d); }
  static std::string ToString(const EnumDescriptor* d) { return Payload(d); }
  static std::string ToString(const EnumValueDescriptor* d) {
    return Payload(d);
  }
  static std::string ToString(const OneofDescriptor* d) { return Payload(d); }

  template <typename Descriptor>
  static std::string Payload(const Descriptor* descriptor) {
    std::vector<int> path;
    descriptor->GetLocationPath(&path);
    GeneratedCodeInfo::Annotation annotation;
    for (int i = 0; i < path.size(); ++i) {
      annotation.add_path(path[i]);
    }
    annotation.set_source_file(descriptor->file()->name());
    return annotation.SerializeAsString();
  }
};

class PROTOC_EXPORT NamespaceOpener {
 public:
  explicit NamespaceOpener(const Formatter& format)
      : printer_(format.printer()) {}
  NamespaceOpener(const std::string& name, const Formatter& format)
      : NamespaceOpener(format) {
    ChangeTo(name);
  }
  ~NamespaceOpener() { ChangeTo(""); }

  void ChangeTo(const std::string& name) {
    std::vector<std::string> new_stack_ =
        Split(name, "::", true);
    int len = std::min(name_stack_.size(), new_stack_.size());
    int common_idx = 0;
    while (common_idx < len) {
      if (name_stack_[common_idx] != new_stack_[common_idx]) break;
      common_idx++;
    }
    for (int i = name_stack_.size() - 1; i >= common_idx; i--) {
      if (name_stack_[i] == "PROTOBUF_NAMESPACE_ID") {
        printer_->Print("PROTOBUF_NAMESPACE_CLOSE\n");
      } else {
        printer_->Print("}  // namespace $ns$\n", "ns", name_stack_[i]);
      }
    }
    name_stack_.swap(new_stack_);
    for (int i = common_idx; i < name_stack_.size(); i++) {
      if (name_stack_[i] == "PROTOBUF_NAMESPACE_ID") {
        printer_->Print("PROTOBUF_NAMESPACE_OPEN\n");
      } else {
        printer_->Print("namespace $ns$ {\n", "ns", name_stack_[i]);
      }
    }
  }

 private:
  io::Printer* printer_;
  std::vector<std::string> name_stack_;
};

std::string GetUtf8Suffix(const FieldDescriptor* field, const Options& options);
void GenerateUtf8CheckCodeForString(const FieldDescriptor* field,
                                    const Options& options, bool for_parse,
                                    const char* parameters,
                                    const Formatter& format);

void GenerateUtf8CheckCodeForCord(const FieldDescriptor* field,
                                  const Options& options, bool for_parse,
                                  const char* parameters,
                                  const Formatter& format);

template <typename T>
struct FieldRangeImpl {
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = const FieldDescriptor*;
    using difference_type = int;

    value_type operator*() { return descriptor->field(idx); }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      GOOGLE_DCHECK(a.descriptor == b.descriptor);
      return a.idx == b.idx;
    }
    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return !(a == b);
    }

    Iterator& operator++() {
      idx++;
      return *this;
    }

    int idx;
    const T* descriptor;
  };

  Iterator begin() const { return {0, descriptor}; }
  Iterator end() const { return {descriptor->field_count(), descriptor}; }

  const T* descriptor;
};

template <typename T>
FieldRangeImpl<T> FieldRange(const T* desc) {
  return {desc};
}

struct OneOfRangeImpl {
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = const OneofDescriptor*;
    using difference_type = int;

    value_type operator*() { return descriptor->oneof_decl(idx); }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      GOOGLE_DCHECK(a.descriptor == b.descriptor);
      return a.idx == b.idx;
    }
    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return !(a == b);
    }

    Iterator& operator++() {
      idx++;
      return *this;
    }

    int idx;
    const Descriptor* descriptor;
  };

  Iterator begin() const { return {0, descriptor}; }
  Iterator end() const { return {descriptor->oneof_decl_count(), descriptor}; }

  const Descriptor* descriptor;
};

inline OneOfRangeImpl OneOfRange(const Descriptor* desc) { return {desc}; }

void GenerateParserLoop(const Descriptor* descriptor, int num_hasbits,
                        const Options& options,
                        MessageSCCAnalyzer* scc_analyzer, io::Printer* printer);

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_COMPILER_CPP_HELPERS_H__
