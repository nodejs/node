// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_PRINTER_H_
#define V8_REGEXP_REGEXP_PRINTER_H_

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS
#include <iostream>
#include <map>
#include <memory>

#include "src/base/logging.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {

template <typename T>
class RegExpNodePrinter;
class RegExpGraphPrinter;
class RegExpNode;
class RegExpTree;
class TraceRegExpTreeScope;
class Zone;

template <typename T>
class RegExpGraphLabeller {
 public:
  auto RegisterNode(const T* node) {
    auto result = nodes_.emplace(node, next_node_label_);
    if (result.second) {
      next_node_label_++;
    }
    return result.first;
  }

  int NodeId(const T* node) {
    auto it = nodes_.find(node);
    if (it == nodes_.end()) {
      it = RegisterNode(node);
      DCHECK_NE(it, nodes_.end());
    }
    return it->second;
  }

  int NodeId(const T* node) const {
    auto it = nodes_.find(node);
    if (it == nodes_.end()) {
      return -1;
    }
    return it->second;
  }

 private:
  std::map<const T*, int> nodes_;
  int next_node_label_ = 1;
};

class RegExpPrinterBase {
 public:
  enum class Color : int {
    kDefault = 0,
    kRed = 31,
    kGreen = 32,
    kYellow = 33,
    kBlue = 34,
    kMagenta = 35,
    kCyan = 36,
    kWhite = 37
  };

  RegExpPrinterBase(std::ostream& os, Zone* zone) : os_(os), zone_(zone) {}
  RegExpPrinterBase(const RegExpPrinterBase& other) V8_NOEXCEPT = default;
  int color() const { return color_; }
  void set_color(Color color) {
    if (!v8_flags.log_colour) return;
    color_ = static_cast<int>(color);
    os() << "\033[" << color_ << "m";
  }
  void set_bold(bool bold) {
    if (!v8_flags.log_colour) return;
    os() << "\033[" << (bold ? "1" : "0") << ";" << color() << "m";
  }
  std::ostream& os() { return os_; }
  Zone* zone() { return zone_; }

 private:
  std::ostream& os_;
  Zone* zone_;
  int color_ = 0;
};

template <typename T>
class RegExpNodePrinterBase : public RegExpPrinterBase {
 public:
  RegExpNodePrinterBase(std::ostream& os, RegExpGraphLabeller<T>* labeller,
                        Zone* zone, char prefix)
      : RegExpPrinterBase(os, zone), labeller_(labeller), prefix_(prefix) {}
  RegExpNodePrinterBase(const RegExpPrinterBase& other,
                        RegExpGraphLabeller<T>* labeller, char prefix)
      : RegExpPrinterBase(other), labeller_(labeller), prefix_(prefix) {}

  RegExpGraphLabeller<T>* labeller() { return labeller_; }
  const RegExpGraphLabeller<T>* labeller() const { return labeller_; }

  void PrintNodeLabel(const T* node) {
    if (!labeller_) return;
    int id = labeller_->NodeId(node);
    DCHECK_NE(id, 0);
    set_bold(true);
    os() << prefix_ << id;
    set_bold(false);
  }
  void RegisterNode(const T* node) {
    if (!labeller_) return;
    labeller_->RegisterNode(node);
  }

 protected:
  RegExpGraphLabeller<T>* labeller_;
  const char prefix_;
};

class RegExpDiagnostics {
 public:
  RegExpDiagnostics(std::ostream& os, Zone* zone);
  ~RegExpDiagnostics();

  std::ostream& os() const { return os_; }
  Zone* zone() const { return zone_; }

  bool has_graph_labeller() const { return !!graph_labeller_; }
  RegExpGraphLabeller<RegExpNode>* graph_labeller() const {
    DCHECK(has_graph_labeller());
    return graph_labeller_.get();
  }
  void set_graph_labeller(
      std::unique_ptr<RegExpGraphLabeller<RegExpNode>> graph_labeller);

  bool has_tree_labeller() const { return !!tree_labeller_; }
  RegExpGraphLabeller<RegExpTree>* tree_labeller() const {
    DCHECK(has_tree_labeller());
    return tree_labeller_.get();
  }
  void set_tree_labeller(
      std::unique_ptr<RegExpGraphLabeller<RegExpTree>> tree_labeller);

  bool has_graph_printer() const { return !!graph_printer_; }
  RegExpGraphPrinter* graph_printer() const {
    DCHECK(has_graph_printer());
    return graph_printer_.get();
  }
  void set_graph_printer(std::unique_ptr<RegExpGraphPrinter> graph_printer);

  bool has_ast_printer() const { return !!ast_printer_; }
  RegExpNodePrinter<RegExpTree>* ast_printer() const {
    DCHECK(has_ast_printer());
    return ast_printer_.get();
  }
  void set_ast_printer(
      std::unique_ptr<RegExpNodePrinter<RegExpTree>> ast_printer);

  TraceRegExpTreeScope* trace_tree_scope() { return trace_tree_scope_; }
  void set_trace_tree_scope(TraceRegExpTreeScope* scope) {
    trace_tree_scope_ = scope;
  }

 private:
  std::unique_ptr<RegExpGraphLabeller<RegExpNode>> graph_labeller_;
  std::unique_ptr<RegExpGraphLabeller<RegExpTree>> tree_labeller_;
  std::unique_ptr<RegExpGraphPrinter> graph_printer_;
  std::unique_ptr<RegExpNodePrinter<RegExpTree>> ast_printer_;
  TraceRegExpTreeScope* trace_tree_scope_ = nullptr;
  std::ostream& os_;
  Zone* zone_;
};

}  // namespace internal
}  // namespace v8
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

#endif  // V8_REGEXP_REGEXP_PRINTER_H_
