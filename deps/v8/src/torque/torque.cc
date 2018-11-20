// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "./antlr4-runtime.h"
#include "src/torque/TorqueBaseVisitor.h"
#include "src/torque/TorqueLexer.h"
#include "src/torque/ast-generator.h"
#include "src/torque/declarable.h"
#include "src/torque/declaration-visitor.h"
#include "src/torque/global-context.h"
#include "src/torque/implementation-visitor.h"
#include "src/torque/scope.h"
#include "src/torque/type-oracle.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

size_t Label::next_id_ = 0;

class FailedParseErrorStrategy : public antlr4::DefaultErrorStrategy {
 public:
  FailedParseErrorStrategy() : DefaultErrorStrategy(), failed_(false) {}
  void reportError(antlr4::Parser* recognizer,
                   const antlr4::RecognitionException& e) override {
    antlr4::DefaultErrorStrategy::reportError(recognizer, e);
    failed_ = true;
  }

  bool FailedParse() const { return failed_; }

 public:
  bool failed_;
};

class TorqueErrorListener : public antlr4::BaseErrorListener {
 public:
  TorqueErrorListener() : BaseErrorListener() {}

  void syntaxError(antlr4::Recognizer* recognizer,
                   antlr4::Token* /*offendingSymbol*/, size_t line,
                   size_t charPositionInLine, const std::string& msg,
                   std::exception_ptr /*e*/) {
    std::cerr << recognizer->getInputStream()->getSourceName() << ": " << line
              << ":" << charPositionInLine << " " << msg << "\n";
  }
};

int WrappedMain(int argc, const char** argv) {
  std::string output_directory;
  std::vector<SourceFileContext> file_contexts;
  AstGenerator ast_generator;
  SourceFileContext context;
  size_t lexer_errors = 0;
  auto error_strategy = std::make_shared<FailedParseErrorStrategy>();
  TorqueErrorListener error_listener;
  bool verbose = false;
  SourceFileMap::Scope scope;
  for (int i = 1; i < argc; ++i) {
    // Check for options
    if (!strcmp("-o", argv[i])) {
      output_directory = argv[++i];
      continue;
    }
    if (!strcmp("-v", argv[i])) {
      verbose = true;
      continue;
    }

    // Otherwise it's a .tq
    // file, parse it and
    // remember the syntax tree
    context.name = argv[i];
    context.stream = std::unique_ptr<antlr4::ANTLRFileStream>(
        new antlr4::ANTLRFileStream(context.name.c_str()));
    context.lexer =
        std::unique_ptr<TorqueLexer>(new TorqueLexer(context.stream.get()));
    context.lexer->removeErrorListeners();
    context.lexer->addErrorListener(&error_listener);
    context.tokens = std::unique_ptr<antlr4::CommonTokenStream>(
        new antlr4::CommonTokenStream(context.lexer.get()));
    context.tokens->fill();
    lexer_errors += context.lexer->getNumberOfSyntaxErrors();
    context.parser =
        std::unique_ptr<TorqueParser>(new TorqueParser(context.tokens.get()));
    context.parser->setErrorHandler(error_strategy);
    context.parser->removeErrorListeners();
    context.parser->addErrorListener(&error_listener);
    context.file = context.parser->file();
    ast_generator.visitSourceFile(&context);
  }

  if (lexer_errors != 0 || error_strategy->FailedParse()) {
    return -1;
  }

  GlobalContext global_context(std::move(ast_generator).GetAst());
  if (verbose) global_context.SetVerbose();
  TypeOracle::Scope type_oracle(global_context.declarations());

  if (output_directory.length() != 0) {
    {
      DeclarationVisitor visitor(global_context);

      visitor.Visit(global_context.ast());

      std::string output_header_path = output_directory;
      output_header_path += "/builtin-definitions-from-dsl.h";
      visitor.GenerateHeader(output_header_path);
    }

    ImplementationVisitor visitor(global_context);
    for (auto& module : global_context.GetModules()) {
      visitor.BeginModuleFile(module.second.get());
    }

    visitor.Visit(global_context.ast());

    for (auto& module : global_context.GetModules()) {
      visitor.EndModuleFile(module.second.get());
      visitor.GenerateImplementation(output_directory, module.second.get());
    }
  }
  return 0;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

int main(int argc, const char** argv) {
  return v8::internal::torque::WrappedMain(argc, argv);
}
