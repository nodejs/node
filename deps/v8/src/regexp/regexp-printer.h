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

class Zone;

namespace regexp {

class GraphPrinter;
class Node;
template <typename T>
class NodePrinter;
class TraceTreeScope;
class Tree;

template <typename T>
class GraphLabeller {
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

class PrinterBase {
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

  PrinterBase(std::ostream& os, Zone* zone) : os_(os), zone_(zone) {}
  PrinterBase(const PrinterBase& other) V8_NOEXCEPT = default;
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
class NodePrinterBase : public PrinterBase {
 public:
  NodePrinterBase(std::ostream& os, GraphLabeller<T>* labeller, Zone* zone,
                  char prefix)
      : PrinterBase(os, zone), labeller_(labeller), prefix_(prefix) {}
  NodePrinterBase(const PrinterBase& other, GraphLabeller<T>* labeller,
                  char prefix)
      : PrinterBase(other), labeller_(labeller), prefix_(prefix) {}

  GraphLabeller<T>* labeller() { return labeller_; }
  const GraphLabeller<T>* labeller() const { return labeller_; }

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
  GraphLabeller<T>* labeller_;
  const char prefix_;
};

class Diagnostics {
 public:
  Diagnostics(std::ostream& os, Zone* zone);
  ~Diagnostics();

  std::ostream& os() const { return os_; }
  Zone* zone() const { return zone_; }

  bool has_graph_labeller() const { return !!graph_labeller_; }
  GraphLabeller<Node>* graph_labeller() const {
    DCHECK(has_graph_labeller());
    return graph_labeller_.get();
  }
  void set_graph_labeller(std::unique_ptr<GraphLabeller<Node>> graph_labeller);

  bool has_tree_labeller() const { return !!tree_labeller_; }
  GraphLabeller<Tree>* tree_labeller() const {
    DCHECK(has_tree_labeller());
    return tree_labeller_.get();
  }
  void set_tree_labeller(std::unique_ptr<GraphLabeller<Tree>> tree_labeller);

  bool has_graph_printer() const { return !!graph_printer_; }
  GraphPrinter* graph_printer() const {
    DCHECK(has_graph_printer());
    return graph_printer_.get();
  }
  void set_graph_printer(std::unique_ptr<GraphPrinter> graph_printer);

  bool has_ast_printer() const { return !!ast_printer_; }
  NodePrinter<Tree>* ast_printer() const {
    DCHECK(has_ast_printer());
    return ast_printer_.get();
  }
  void set_ast_printer(std::unique_ptr<NodePrinter<Tree>> ast_printer);

  TraceTreeScope* trace_tree_scope() { return trace_tree_scope_; }
  void set_trace_tree_scope(TraceTreeScope* scope) {
    trace_tree_scope_ = scope;
  }

 private:
  std::unique_ptr<GraphLabeller<Node>> graph_labeller_;
  std::unique_ptr<GraphLabeller<Tree>> tree_labeller_;
  std::unique_ptr<GraphPrinter> graph_printer_;
  std::unique_ptr<NodePrinter<Tree>> ast_printer_;
  TraceTreeScope* trace_tree_scope_ = nullptr;
  std::ostream& os_;
  Zone* zone_;
};

}  // namespace regexp
}  // namespace internal
}  // namespace v8
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

#endif  // V8_REGEXP_REGEXP_PRINTER_H_
