// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/fast/requirements.h"

#include <algorithm>
#include <utility>

#include "experimental/redline/fast/ast_visit.h"

namespace redline {
namespace fast {

namespace {

bool IsLegacyOctal(std::string_view raw) {
  if (raw.size() < 2 || raw[0] != '0') {
    return false;
  }
  return raw[1] >= '0' && raw[1] <= '9';
}

class RequirementsAnalyzer {
 public:
  Requirements Analyze(Node* root) {
    req_ = Requirements();
    if (root == nullptr) {
      return req_;
    }
    if (root->Is<VariableDeclaration>()) {
      VariableKind k = root->Cast<VariableDeclaration>()->declaration_kind;
      req_.has_lexical_declaration = k != VariableKind::kVar;
    } else if (root->Is<ClassDeclaration>()) {
      req_.has_lexical_declaration = true;
    }
    Visit(root, 0, 0, 0, 0);
    return req_;
  }

 private:
  void Visit(Node* node, int fn_depth, int this_fn_depth, int loop_depth,
             int switch_depth) {
    if (node == nullptr) {
      return;
    }
    switch (node->kind) {
      case NodeKind::kReturnStatement:
        if (fn_depth == 0) {
          req_.needs_function = true;
        }
        break;
      case NodeKind::kAwaitExpression:
        if (fn_depth == 0) {
          req_.needs_async = true;
        }
        break;
      case NodeKind::kYieldExpression:
        if (fn_depth == 0) {
          req_.needs_generator = true;
        }
        break;
      case NodeKind::kForOfStatement:
        if (node->Cast<ForOfStatement>()->is_await && fn_depth == 0) {
          req_.needs_async = true;
        }
        break;
      case NodeKind::kBreakStatement: {
        Identifier* label = node->Cast<BreakStatement>()->label;
        if (label != nullptr) {
          if (!HasLabel(label->name)) {
            req_.has_external_label = true;
          }
        } else if (loop_depth == 0 && switch_depth == 0) {
          req_.needs_break = true;
        }
        return;
      }
      case NodeKind::kContinueStatement: {
        Identifier* label = node->Cast<ContinueStatement>()->label;
        if (label != nullptr) {
          if (!HasLabel(label->name)) {
            req_.has_external_label = true;
          }
        } else if (loop_depth == 0) {
          req_.needs_continue = true;
        }
        return;
      }
      case NodeKind::kWithStatement:
        req_.needs_sloppy = true;
        break;
      case NodeKind::kUnaryExpression: {
        auto* u = node->Cast<UnaryExpression>();
        if (u->op == UnaryOp::kDelete && u->argument->Is<Identifier>()) {
          req_.needs_sloppy = true;
        }
        break;
      }
      case NodeKind::kNumericLiteral:
        if (IsLegacyOctal(node->Cast<NumericLiteral>()->raw)) {
          req_.needs_sloppy = true;
        }
        return;
      case NodeKind::kThisExpression:
        if (this_fn_depth == 0) {
          req_.needs_this = true;
        }
        return;
      case NodeKind::kIdentifier:
        if (node->Cast<Identifier>()->name == "arguments" &&
            this_fn_depth == 0) {
          req_.needs_arguments = true;
        }
        return;
      case NodeKind::kSuper:
        if (this_fn_depth == 0) {
          req_.needs_super = true;
        }
        return;
      case NodeKind::kMetaProperty:
        if (node->Cast<MetaProperty>()->meta_kind ==
                MetaPropertyKind::kNewTarget &&
            this_fn_depth == 0) {
          req_.needs_new_target = true;
        }
        return;
      case NodeKind::kFunctionDeclaration:
      case NodeKind::kFunctionExpression:
      case NodeKind::kArrowFunctionExpression: {
        const FunctionData* fn = AsFunctionData(node);
        int next_this = this_fn_depth + (fn->is_arrow ? 0 : 1);
        std::vector<std::string_view> saved = std::move(labels_);
        labels_.clear();
        ForEachChild(node, [&](Node* child) {
          Visit(child, fn_depth + 1, next_this, 0, 0);
        });
        labels_ = std::move(saved);
        return;
      }
      case NodeKind::kPropertyDefinition: {
        auto* p = node->Cast<PropertyDefinition>();
        // A computed key runs in the enclosing scope. The initializer runs
        // as a method of the class.
        if (p->computed) {
          Visit(p->key, fn_depth, this_fn_depth, loop_depth, switch_depth);
        }
        std::vector<std::string_view> saved = std::move(labels_);
        labels_.clear();
        if (!p->computed) {
          Visit(p->key, fn_depth + 1, this_fn_depth + 1, 0, 0);
        }
        Visit(p->value, fn_depth + 1, this_fn_depth + 1, 0, 0);
        labels_ = std::move(saved);
        return;
      }
      case NodeKind::kStaticBlock: {
        std::vector<std::string_view> saved = std::move(labels_);
        labels_.clear();
        ForEachChild(node, [&](Node* child) {
          Visit(child, fn_depth + 1, this_fn_depth + 1, 0, 0);
        });
        labels_ = std::move(saved);
        return;
      }
      case NodeKind::kLabeledStatement: {
        auto* l = node->Cast<LabeledStatement>();
        if (fn_depth == 0) {
          req_.declared_labels.push_back(l->label->name);
        }
        labels_.push_back(l->label->name);
        Visit(l->body, fn_depth, this_fn_depth, loop_depth, switch_depth);
        labels_.pop_back();
        return;
      }
      case NodeKind::kWhileStatement:
      case NodeKind::kDoWhileStatement:
      case NodeKind::kForStatement:
      case NodeKind::kForInStatement:
        ForEachChild(node, [&](Node* child) {
          Visit(child, fn_depth, this_fn_depth, loop_depth + 1, switch_depth);
        });
        return;
      case NodeKind::kSwitchStatement:
        ForEachChild(node, [&](Node* child) {
          Visit(child, fn_depth, this_fn_depth, loop_depth, switch_depth + 1);
        });
        return;
      default:
        break;
    }
    if (node->Is<ForOfStatement>()) {
      ForEachChild(node, [&](Node* child) {
        Visit(child, fn_depth, this_fn_depth, loop_depth + 1, switch_depth);
      });
      return;
    }
    ForEachChild(node, [&](Node* child) {
      Visit(child, fn_depth, this_fn_depth, loop_depth, switch_depth);
    });
  }

  bool HasLabel(std::string_view name) const {
    return std::find(labels_.begin(), labels_.end(), name) != labels_.end();
  }

  Requirements req_;
  std::vector<std::string_view> labels_;
};

}  // namespace

Requirements AnalyzeRequirements(Node* node) {
  RequirementsAnalyzer analyzer;
  return analyzer.Analyze(node);
}

}  // namespace fast
}  // namespace redline
