// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_LS_MESSAGE_H_
#define V8_TORQUE_LS_MESSAGE_H_

#include "src/base/logging.h"
#include "src/torque/ls/json.h"
#include "src/torque/ls/message-macros.h"
#include "src/torque/source-positions.h"

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

// Base class for Messages and Objects that are backed by either a
// JsonValue or a reference to a JsonObject.
// Helper methods are used by macros to implement typed accessors.
class BaseJsonAccessor {
 public:
  template <class T>
  T GetObject(const std::string& property) {
    return T(GetObjectProperty(property));
  }

  bool HasProperty(const std::string& property) const {
    return object().count(property) > 0;
  }

  void SetNull(const std::string& property) {
    object()[property] = JsonValue::JsonNull();
  }

  bool IsNull(const std::string& property) const {
    return HasProperty(property) &&
           object().at(property).tag == JsonValue::IS_NULL;
  }

 protected:
  virtual const JsonObject& object() const = 0;
  virtual JsonObject& object() = 0;

  JsonObject& GetObjectProperty(const std::string& property) {
    if (!object()[property].IsObject()) {
      object()[property] = JsonValue::From(JsonObject{});
    }
    return object()[property].ToObject();
  }

  JsonArray& GetArrayProperty(const std::string& property) {
    if (!object()[property].IsArray()) {
      object()[property] = JsonValue::From(JsonArray{});
    }
    return object()[property].ToArray();
  }

  JsonObject& AddObjectElementToArrayProperty(const std::string& property) {
    JsonArray& array = GetArrayProperty(property);
    array.push_back(JsonValue::From(JsonObject{}));

    return array.back().ToObject();
  }
};

// Base class for Requests, Responses and Notifications.
// In contrast to "BaseObject", a Message owns the backing JsonValue of the
// whole object tree; i.e. value_ serves as root.
class Message : public BaseJsonAccessor {
 public:
  Message() {
    value_ = JsonValue::From(JsonObject{});
    set_jsonrpc("2.0");
  }
  explicit Message(JsonValue& value) : value_(std::move(value)) {
    CHECK(value_.tag == JsonValue::OBJECT);
  }

  JsonValue& GetJsonValue() { return value_; }

  JSON_STRING_ACCESSORS(jsonrpc)

 protected:
  const JsonObject& object() const { return value_.ToObject(); }
  JsonObject& object() { return value_.ToObject(); }

 private:
  JsonValue value_;
};

// Base class for complex type that might be part of a Message.
// Instead of creating theses directly, use the accessors on the
// root Message or a parent object.
class NestedJsonAccessor : public BaseJsonAccessor {
 public:
  explicit NestedJsonAccessor(JsonObject& object) : object_(object) {}

  const JsonObject& object() const { return object_; }
  JsonObject& object() { return object_; }

 private:
  JsonObject& object_;
};

class ResponseError : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_INT_ACCESSORS(code)
  JSON_STRING_ACCESSORS(message)
};

class InitializeParams : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_INT_ACCESSORS(processId)
  JSON_STRING_ACCESSORS(rootPath)
  JSON_STRING_ACCESSORS(rootUri)
  JSON_STRING_ACCESSORS(trace)
};

class FileListParams : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  // TODO(szuend): Implement read accessor for string
  //               arrays. "files" is managed directly.
};

class FileSystemWatcher : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_STRING_ACCESSORS(globPattern)
  JSON_INT_ACCESSORS(kind)

  enum WatchKind {
    kCreate = 1,
    kChange = 2,
    kDelete = 4,

    kAll = kCreate | kChange | kDelete,
  };
};

class DidChangeWatchedFilesRegistrationOptions : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_ARRAY_OBJECT_ACCESSORS(FileSystemWatcher, watchers)
};

class FileEvent : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_STRING_ACCESSORS(uri)
  JSON_INT_ACCESSORS(type)
};

class DidChangeWatchedFilesParams : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_ARRAY_OBJECT_ACCESSORS(FileEvent, changes)
};

class SaveOptions : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_BOOL_ACCESSORS(includeText)
};

class TextDocumentSyncOptions : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_BOOL_ACCESSORS(openClose)
  JSON_INT_ACCESSORS(change)
  JSON_BOOL_ACCESSORS(willSave)
  JSON_BOOL_ACCESSORS(willSaveWaitUntil)
  JSON_OBJECT_ACCESSORS(SaveOptions, save)
};

class ServerCapabilities : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_OBJECT_ACCESSORS(TextDocumentSyncOptions, textDocumentSync)
  JSON_BOOL_ACCESSORS(definitionProvider)
  JSON_BOOL_ACCESSORS(documentSymbolProvider)
};

class InitializeResult : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_OBJECT_ACCESSORS(ServerCapabilities, capabilities)
};

class Registration : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_STRING_ACCESSORS(id)
  JSON_STRING_ACCESSORS(method)
  JSON_DYNAMIC_OBJECT_ACCESSORS(registerOptions)
};

class RegistrationParams : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_ARRAY_OBJECT_ACCESSORS(Registration, registrations)
};

class JsonPosition : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_INT_ACCESSORS(line)
  JSON_INT_ACCESSORS(character)
};

class Range : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_OBJECT_ACCESSORS(JsonPosition, start)
  JSON_OBJECT_ACCESSORS(JsonPosition, end)
};

class Location : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_STRING_ACCESSORS(uri)
  JSON_OBJECT_ACCESSORS(Range, range)

  void SetTo(SourcePosition position) {
    set_uri(SourceFileMap::GetSource(position.source));
    range().start().set_line(position.start.line);
    range().start().set_character(position.start.column);
    range().end().set_line(position.end.line);
    range().end().set_character(position.end.column);
  }
};

class TextDocumentIdentifier : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_STRING_ACCESSORS(uri)
};

class TextDocumentPositionParams : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_OBJECT_ACCESSORS(TextDocumentIdentifier, textDocument)
  JSON_OBJECT_ACCESSORS(JsonPosition, position)
};

class Diagnostic : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  enum DiagnosticSeverity {
    kError = 1,
    kWarning = 2,
    kInformation = 3,
    kHint = 4
  };

  JSON_OBJECT_ACCESSORS(Range, range)
  JSON_INT_ACCESSORS(severity)
  JSON_STRING_ACCESSORS(source)
  JSON_STRING_ACCESSORS(message)
};

class PublishDiagnosticsParams : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_STRING_ACCESSORS(uri)
  JSON_ARRAY_OBJECT_ACCESSORS(Diagnostic, diagnostics)
};

enum SymbolKind {
  kFile = 1,
  kNamespace = 3,
  kClass = 5,
  kMethod = 6,
  kProperty = 7,
  kField = 8,
  kConstructor = 9,
  kFunction = 12,
  kVariable = 13,
  kConstant = 14,
  kStruct = 23,
};

class DocumentSymbolParams : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_OBJECT_ACCESSORS(TextDocumentIdentifier, textDocument)
};

class SymbolInformation : public NestedJsonAccessor {
 public:
  using NestedJsonAccessor::NestedJsonAccessor;

  JSON_STRING_ACCESSORS(name)
  JSON_INT_ACCESSORS(kind)
  JSON_OBJECT_ACCESSORS(Location, location)
  JSON_STRING_ACCESSORS(containerName)
};

template <class T>
class Request : public Message {
 public:
  explicit Request(JsonValue& value) : Message(value) {}
  Request() : Message() {}

  JSON_INT_ACCESSORS(id)
  JSON_STRING_ACCESSORS(method)
  JSON_OBJECT_ACCESSORS(T, params)
};
using InitializeRequest = Request<InitializeParams>;
using RegistrationRequest = Request<RegistrationParams>;
using TorqueFileListNotification = Request<FileListParams>;
using GotoDefinitionRequest = Request<TextDocumentPositionParams>;
using DidChangeWatchedFilesNotification = Request<DidChangeWatchedFilesParams>;
using PublishDiagnosticsNotification = Request<PublishDiagnosticsParams>;
using DocumentSymbolRequest = Request<DocumentSymbolParams>;

template <class T>
class Response : public Message {
 public:
  explicit Response(JsonValue& value) : Message(value) {}
  Response() : Message() {}

  JSON_INT_ACCESSORS(id)
  JSON_OBJECT_ACCESSORS(ResponseError, error)
  JSON_OBJECT_ACCESSORS(T, result)
};
using InitializeResponse = Response<InitializeResult>;
using GotoDefinitionResponse = Response<Location>;

// Same as "Response" but the result is T[] instead of T.
template <class T>
class ResponseArrayResult : public Message {
 public:
  explicit ResponseArrayResult(JsonValue& value) : Message(value) {}
  ResponseArrayResult() : Message() {}

  JSON_INT_ACCESSORS(id)
  JSON_OBJECT_ACCESSORS(ResponseError, error)
  JSON_ARRAY_OBJECT_ACCESSORS(T, result)
};
using DocumentSymbolResponse = ResponseArrayResult<SymbolInformation>;

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_LS_MESSAGE_H_
