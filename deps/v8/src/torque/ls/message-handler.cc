// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include "src/torque/ls/message-handler.h"

#include "src/torque/ls/globals.h"
#include "src/torque/ls/json-parser.h"
#include "src/torque/ls/message-pipe.h"
#include "src/torque/ls/message.h"
#include "src/torque/server-data.h"
#include "src/torque/source-positions.h"
#include "src/torque/torque-compiler.h"

namespace v8 {
namespace internal {
namespace torque {

DEFINE_CONTEXTUAL_VARIABLE(Logger)
DEFINE_CONTEXTUAL_VARIABLE(TorqueFileList)
DEFINE_CONTEXTUAL_VARIABLE(DiagnosticsFiles)

namespace ls {

static const char kContentLength[] = "Content-Length: ";
static const size_t kContentLengthSize = sizeof(kContentLength) - 1;

#ifdef V8_OS_WIN
// On Windows, in text mode, \n is translated to \r\n.
constexpr const char* kProtocolLineEnding = "\n\n";
#else
constexpr const char* kProtocolLineEnding = "\r\n\r\n";
#endif

JsonValue ReadMessage() {
  std::string line;
  std::getline(std::cin, line);

  if (line.rfind(kContentLength) != 0) {
    // Invalid message, we just crash.
    Logger::Log("[fatal] Did not find Content-Length ...\n");
    v8::base::OS::Abort();
  }

  const int content_length = std::atoi(line.substr(kContentLengthSize).c_str());
  std::getline(std::cin, line);
  std::string content(content_length, ' ');
  std::cin.read(&content[0], content_length);

  Logger::Log("[incoming] ", content, "\n\n");

  return ParseJson(content).value;
}

void WriteMessage(JsonValue message) {
  std::string content = SerializeToString(message);

  Logger::Log("[outgoing] ", content, "\n\n");

  std::cout << kContentLength << content.size() << kProtocolLineEnding;
  std::cout << content << std::flush;
}

namespace {

void ResetCompilationErrorDiagnostics(MessageWriter writer) {
  for (const SourceId& source : DiagnosticsFiles::Get()) {
    PublishDiagnosticsNotification notification;
    notification.set_method("textDocument/publishDiagnostics");

    std::string error_file = SourceFileMap::AbsolutePath(source);
    notification.params().set_uri(error_file);
    // Trigger empty array creation.
    USE(notification.params().diagnostics_size());

    writer(std::move(notification.GetJsonValue()));
  }
  DiagnosticsFiles::Get() = {};
}

// Each notification must contain all diagnostics for a specific file,
// because sending multiple notifications per file resets previously sent
// diagnostics. Thus, two steps are needed:
//   1) collect all notifications in this class.
//   2) send one notification per entry (per file).
class DiagnosticCollector {
 public:
  void AddTorqueMessage(const TorqueMessage& message) {
    SourceId id =
        message.position ? message.position->source : SourceId::Invalid();
    auto& notification = GetOrCreateNotificationForSource(id);

    Diagnostic diagnostic = notification.params().add_diagnostics();
    diagnostic.set_severity(ServerityFor(message.kind));
    diagnostic.set_message(message.message);
    diagnostic.set_source("Torque Compiler");

    if (message.position) {
      PopulateRangeFromSourcePosition(diagnostic.range(), *message.position);
    }
  }

  std::map<SourceId, PublishDiagnosticsNotification>& notifications() {
    return notifications_;
  }

 private:
  PublishDiagnosticsNotification& GetOrCreateNotificationForSource(
      SourceId id) {
    auto iter = notifications_.find(id);
    if (iter != notifications_.end()) return iter->second;

    PublishDiagnosticsNotification& notification = notifications_[id];
    notification.set_method("textDocument/publishDiagnostics");

    std::string file =
        id.IsValid() ? SourceFileMap::AbsolutePath(id) : "<unknown>";
    notification.params().set_uri(file);
    return notification;
  }

  void PopulateRangeFromSourcePosition(Range range,
                                       const SourcePosition& position) {
    range.start().set_line(position.start.line);
    range.start().set_character(position.start.column);
    range.end().set_line(position.end.line);
    range.end().set_character(position.end.column);
  }

  Diagnostic::DiagnosticSeverity ServerityFor(TorqueMessage::Kind kind) {
    switch (kind) {
      case TorqueMessage::Kind::kError:
        return Diagnostic::kError;
      case TorqueMessage::Kind::kLint:
        return Diagnostic::kWarning;
    }
  }

  std::map<SourceId, PublishDiagnosticsNotification> notifications_;
};

void SendCompilationDiagnostics(const TorqueCompilerResult& result,
                                MessageWriter writer) {
  DiagnosticCollector collector;

  // TODO(szuend): Split up messages by SourceId and sort them by line number.
  for (const TorqueMessage& message : result.messages) {
    collector.AddTorqueMessage(message);
  }

  for (auto& pair : collector.notifications()) {
    PublishDiagnosticsNotification& notification = pair.second;
    writer(std::move(notification.GetJsonValue()));

    // Record all source files for which notifications are sent, so they
    // can be reset before the next compiler run.
    const SourceId& source = pair.first;
    if (source.IsValid()) DiagnosticsFiles::Get().push_back(source);
  }
}

}  // namespace

void CompilationFinished(TorqueCompilerResult result, MessageWriter writer) {
  LanguageServerData::Get() = std::move(result.language_server_data);
  SourceFileMap::Get() = *result.source_file_map;

  SendCompilationDiagnostics(result, writer);
}

namespace {

void RecompileTorque(MessageWriter writer) {
  Logger::Log("[info] Start compilation run ...\n");

  TorqueCompilerOptions options;
  options.output_directory = "";
  options.collect_language_server_data = true;
  options.force_assert_statements = true;

  TorqueCompilerResult result = CompileTorque(TorqueFileList::Get(), options);

  Logger::Log("[info] Finished compilation run ...\n");

  CompilationFinished(std::move(result), writer);
}

void RecompileTorqueWithDiagnostics(MessageWriter writer) {
  ResetCompilationErrorDiagnostics(writer);
  RecompileTorque(writer);
}

void HandleInitializeRequest(InitializeRequest request, MessageWriter writer) {
  InitializeResponse response;
  response.set_id(request.id());
  response.result().capabilities().textDocumentSync();
  response.result().capabilities().set_definitionProvider(true);
  response.result().capabilities().set_documentSymbolProvider(true);

  // TODO(szuend): Register for document synchronisation here,
  //               so we work with the content that the client
  //               provides, not directly read from files.
  // TODO(szuend): Check that the client actually supports dynamic
  //               "workspace/didChangeWatchedFiles" capability.
  // TODO(szuend): Check if client supports "LocationLink". This will
  //               influence the result of "goto definition".
  writer(std::move(response.GetJsonValue()));
}

void HandleInitializedNotification(MessageWriter writer) {
  RegistrationRequest request;
  // TODO(szuend): The language server needs a "global" request id counter.
  request.set_id(2000);
  request.set_method("client/registerCapability");

  Registration reg = request.params().add_registrations();
  auto options =
      reg.registerOptions<DidChangeWatchedFilesRegistrationOptions>();
  FileSystemWatcher watcher = options.add_watchers();
  watcher.set_globPattern("**/*.tq");
  watcher.set_kind(FileSystemWatcher::WatchKind::kAll);

  reg.set_id("did-change-id");
  reg.set_method("workspace/didChangeWatchedFiles");

  writer(std::move(request.GetJsonValue()));
}

void HandleTorqueFileListNotification(TorqueFileListNotification notification,
                                      MessageWriter writer) {
  CHECK_EQ(notification.params().object()["files"].tag, JsonValue::ARRAY);

  std::vector<std::string>& files = TorqueFileList::Get();
  Logger::Log("[info] Initial file list:\n");
  for (const auto& file_json :
       notification.params().object()["files"].ToArray()) {
    CHECK(file_json.IsString());

    // We only consider file URIs (there shouldn't be anything else).
    // Internally we store the URI instead of the path, eliminating the need
    // to encode it again.
    files.push_back(file_json.ToString());
    Logger::Log("    ", file_json.ToString(), "\n");
  }
  RecompileTorqueWithDiagnostics(writer);
}

void HandleGotoDefinitionRequest(GotoDefinitionRequest request,
                                 MessageWriter writer) {
  GotoDefinitionResponse response;
  response.set_id(request.id());

  SourceId id =
      SourceFileMap::GetSourceId(request.params().textDocument().uri());

  // Unknown source files cause an empty response which corresponds with
  // the definition not beeing found.
  if (!id.IsValid()) {
    response.SetNull("result");
    writer(std::move(response.GetJsonValue()));
    return;
  }

  LineAndColumn pos{request.params().position().line(),
                    request.params().position().character()};

  if (auto maybe_definition = LanguageServerData::FindDefinition(id, pos)) {
    SourcePosition definition = *maybe_definition;
    response.result().SetTo(definition);
  } else {
    response.SetNull("result");
  }

  writer(std::move(response.GetJsonValue()));
}

void HandleChangeWatchedFilesNotification(
    DidChangeWatchedFilesNotification notification, MessageWriter writer) {
  // TODO(szuend): Implement updates to the TorqueFile list when create/delete
  //               notifications are received. Currently we simply re-compile.
  RecompileTorqueWithDiagnostics(writer);
}

void HandleDocumentSymbolRequest(DocumentSymbolRequest request,
                                 MessageWriter writer) {
  DocumentSymbolResponse response;
  response.set_id(request.id());

  SourceId id =
      SourceFileMap::GetSourceId(request.params().textDocument().uri());

  for (const auto& symbol : LanguageServerData::SymbolsForSourceId(id)) {
    DCHECK(symbol->IsUserDefined());
    if (symbol->IsMacro()) {
      Macro* macro = Macro::cast(symbol);
      SymbolInformation symbol = response.add_result();
      symbol.set_name(macro->ReadableName());
      symbol.set_kind(SymbolKind::kFunction);
      symbol.location().SetTo(macro->Position());
    } else if (symbol->IsBuiltin()) {
      Builtin* builtin = Builtin::cast(symbol);
      SymbolInformation symbol = response.add_result();
      symbol.set_name(builtin->ReadableName());
      symbol.set_kind(SymbolKind::kFunction);
      symbol.location().SetTo(builtin->Position());
    } else if (symbol->IsGeneric()) {
      Generic* generic = Generic::cast(symbol);
      SymbolInformation symbol = response.add_result();
      symbol.set_name(generic->name());
      symbol.set_kind(SymbolKind::kFunction);
      symbol.location().SetTo(generic->Position());
    } else if (symbol->IsTypeAlias()) {
      const Type* type = TypeAlias::cast(symbol)->type();
      SymbolKind kind =
          type->IsClassType() ? SymbolKind::kClass : SymbolKind::kStruct;

      SymbolInformation sym = response.add_result();
      sym.set_name(type->ToString());
      sym.set_kind(kind);
      sym.location().SetTo(symbol->Position());
    }
  }

  // Trigger empty array creation in case no symbols were found.
  USE(response.result_size());

  writer(std::move(response.GetJsonValue()));
}

}  // namespace

void HandleMessage(JsonValue raw_message, MessageWriter writer) {
  Request<bool> request(std::move(raw_message));

  // We ignore responses for now. They are matched to requests
  // by id and don't have a method set.
  // TODO(szuend): Implement proper response handling for requests
  //               that originate from the server.
  if (!request.has_method()) {
    Logger::Log("[info] Unhandled response with id ", request.id(), "\n\n");
    return;
  }

  const std::string method = request.method();
  if (method == "initialize") {
    HandleInitializeRequest(
        InitializeRequest(std::move(request.GetJsonValue())), writer);
  } else if (method == "initialized") {
    HandleInitializedNotification(writer);
  } else if (method == "torque/fileList") {
    HandleTorqueFileListNotification(
        TorqueFileListNotification(std::move(request.GetJsonValue())), writer);
  } else if (method == "textDocument/definition") {
    HandleGotoDefinitionRequest(
        GotoDefinitionRequest(std::move(request.GetJsonValue())), writer);
  } else if (method == "workspace/didChangeWatchedFiles") {
    HandleChangeWatchedFilesNotification(
        DidChangeWatchedFilesNotification(std::move(request.GetJsonValue())),
        writer);
  } else if (method == "textDocument/documentSymbol") {
    HandleDocumentSymbolRequest(
        DocumentSymbolRequest(std::move(request.GetJsonValue())), writer);
  } else {
    Logger::Log("[error] Message of type ", method, " is not handled!\n\n");
  }
}

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8
