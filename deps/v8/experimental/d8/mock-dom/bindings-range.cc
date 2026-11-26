// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bindings for Range, Selection, TreeWalker, NodeIterator and
// DOMImplementation.
//
// Ranges are not live: unlike in a browser, their boundary points are not
// adjusted when the tree is mutated behind their back.

#include <algorithm>

#include "experimental/d8/mock-dom/bindings.h"
#include "experimental/d8/mock-dom/css.h"
#include "experimental/d8/mock-dom/html.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/layout.h"
#include "experimental/d8/mock-dom/objects.h"
#include "include/v8-container.h"
#include "include/v8-exception.h"
#include "include/v8-primitive.h"

namespace v8 {
namespace mock_dom {

namespace {

#define CALLBACK(name) void name(const CallbackInfo& info)

void WrapThis(const CallbackInfo& info, ScriptWrappable* object) {
  Object::Wrap<kWrapperTag>(info.GetIsolate(), info.This(), object);
  object->set_wrapper(info.GetIsolate(), info.This());
}

// --- Boundary points ---------------------------------------------------------

uint32_t NodeLength(const Node* node) {
  if (node->IsCharacterData()) {
    return static_cast<const CharacterData*>(node)->data().length();
  }
  uint32_t count = 0;
  for (Node* n = node->first_child(); n; n = n->next_sibling()) ++count;
  return count;
}

uint32_t NodeIndex(const Node* node) {
  uint32_t index = 0;
  for (Node* n = node->previous_sibling(); n; n = n->previous_sibling()) {
    ++index;
  }
  return index;
}

Node* ChildAt(const Node* node, uint32_t index) {
  Node* child = node->first_child();
  while (child && index-- > 0) child = child->next_sibling();
  return child;
}

// Returns -1, 0 or 1 as (a, a_offset) is before, equal to or after
// (b, b_offset). Both must be in the same tree.
int ComparePoints(const Node* a, uint32_t a_offset, const Node* b,
                  uint32_t b_offset) {
  if (a == b) return a_offset < b_offset ? -1 : (a_offset > b_offset ? 1 : 0);
  uint16_t position = b->CompareDocumentPosition(a);
  // |a| follows |b| (and is not its descendant): flip the comparison.
  if ((position & 4) && !(position & 16)) {
    return -ComparePoints(b, b_offset, a, a_offset);
  }
  if (a->Contains(b)) {
    const Node* child = b;
    while (child->parent() != a) child = child->parent();
    if (NodeIndex(child) < a_offset) return 1;
    return -1;
  }
  if (b->Contains(a)) {
    const Node* child = a;
    while (child->parent() != b) child = child->parent();
    if (NodeIndex(child) < b_offset) return -1;
    return 1;
  }
  return -1;
}

std::u16string Utf16Data(const Node* node) {
  const String& data = static_cast<const CharacterData*>(node)->data();
  std::u16string out;
  if (data.is_null()) return out;
  if (data.impl()->is_8bit()) {
    for (char c : data.impl()->view8()) {
      out.push_back(static_cast<unsigned char>(c));
    }
  } else {
    out = std::u16string(data.impl()->view16());
  }
  return out;
}

String FromUtf16(const std::u16string& text) {
  for (char16_t c : text) {
    if (c > 0xFF) return String(StringImpl::Create16(text));
  }
  std::string narrow(text.size(), '\0');
  for (size_t i = 0; i < text.size(); ++i) {
    narrow[i] = static_cast<char>(text[i]);
  }
  return String(StringImpl::Create8(narrow));
}

// --- Range -------------------------------------------------------------------

class Range final : public ScriptWrappable {
 public:
  explicit Range(Document* document)
      : ScriptWrappable(Interface::kRange),
        start_container(document),
        end_container(document) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(start_container);
    visitor->Trace(end_container);
  }
  Window* WrapperWindow() const override {
    return start_container->WrapperWindow();
  }

  bool collapsed() const {
    return start_container == end_container && start_offset == end_offset;
  }

  Node* CommonAncestor() const {
    Node* node = start_container.Get();
    while (node && !node->Contains(end_container.Get())) node = node->parent();
    return node;
  }

  bool SetBoundary(bool start, Node* node, uint32_t offset,
                   ExceptionState* exception_state) {
    if (node->node_type() == Node::kDocumentTypeNode) {
      return exception_state->Throw("InvalidNodeTypeError",
                                    "The node provided is a DocumentType.");
    }
    if (offset > NodeLength(node)) {
      return exception_state->Throw(
          "IndexSizeError", "The offset is larger than the node's length.");
    }
    if (start) {
      start_container = node;
      start_offset = offset;
      if (node->Root() != end_container->Root() ||
          ComparePoints(node, offset, end_container.Get(), end_offset) > 0) {
        end_container = node;
        end_offset = offset;
      }
    } else {
      end_container = node;
      end_offset = offset;
      if (node->Root() != start_container->Root() ||
          ComparePoints(node, offset, start_container.Get(), start_offset) <
              0) {
        start_container = node;
        start_offset = offset;
      }
    }
    return true;
  }

  void Collapse(bool to_start) {
    if (to_start) {
      end_container = start_container;
      end_offset = start_offset;
    } else {
      start_container = end_container;
      start_offset = end_offset;
    }
  }

  // The text content of the range.
  String ToString() const {
    if (start_container == end_container &&
        start_container->IsCharacterData()) {
      std::u16string data = Utf16Data(start_container.Get());
      return FromUtf16(data.substr(start_offset, end_offset - start_offset));
    }
    std::u16string out;
    Node* node = start_container.Get();
    if (node->IsText()) {
      out += Utf16Data(node).substr(start_offset);
      node = node->NextSkippingChildren(nullptr);
    } else {
      Node* child = ChildAt(node, start_offset);
      node = child ? child : node->NextSkippingChildren(nullptr);
    }
    Node* end = end_container.Get();
    Node* stop = end->IsCharacterData() ? end : ChildAt(end, end_offset);
    if (!end->IsCharacterData() && !stop) {
      stop = end->NextSkippingChildren(nullptr);
    }
    for (; node && node != stop; node = node->NextInPreOrder(nullptr)) {
      if (node->IsText()) out += Utf16Data(node);
    }
    if (end->IsText() && node == end) {
      out += Utf16Data(end).substr(0, end_offset);
    }
    return FromUtf16(out);
  }

  // Implements deleteContents (|fragment| null), extractContents and
  // cloneContents (|clone| true).
  void ProcessContents(Isolate* isolate, DocumentFragment* fragment,
                       bool clone) {
    if (collapsed()) return;
    Node* start = start_container.Get();
    Node* end = end_container.Get();
    Document* document =
        start->IsDocument() ? static_cast<Document*>(start) : start->document();
    ExceptionState exception_state;

    auto take_text = [&](Node* node, uint32_t from, uint32_t to,
                         ContainerNode* into) {
      std::u16string data = Utf16Data(node);
      to = std::min<uint32_t>(to, static_cast<uint32_t>(data.size()));
      if (into) {
        Node* copy = node->Clone(isolate, document, false);
        static_cast<CharacterData*>(copy)->SetData(
            isolate, FromUtf16(data.substr(from, to - from)));
        into->AppendChild(isolate, copy, &exception_state);
      }
      if (!clone) {
        data.erase(from, to - from);
        static_cast<CharacterData*>(node)->SetData(isolate, FromUtf16(data));
      }
    };

    if (start == end && start->IsCharacterData()) {
      take_text(start, start_offset, end_offset, fragment);
      if (!clone) Collapse(true);
      return;
    }

    Node* common = CommonAncestor();
    // The children of |common| that are (partially) inside the range.
    Node* first_partial = nullptr;
    if (!start->Contains(end)) {
      first_partial = start;
      while (first_partial->parent() != common) {
        first_partial = first_partial->parent();
      }
    }
    Node* last_partial = nullptr;
    if (!end->Contains(start)) {
      last_partial = end;
      while (last_partial->parent() != common) {
        last_partial = last_partial->parent();
      }
    }
    MOCK_DOM_LOCAL_NODES
    std::vector<Node*> contained;
    {
      Node* child = first_partial ? first_partial->next_sibling()
                                  : ChildAt(common, start_offset);
      Node* stop = last_partial ? last_partial : ChildAt(common, end_offset);
      for (; child && child != stop; child = child->next_sibling()) {
        contained.push_back(child);
      }
    }

    // Where the range collapses to after an extract or delete.
    Node* new_node = start;
    uint32_t new_offset = start_offset;
    if (!start->Contains(end)) {
      Node* reference = start;
      while (reference->parent() && !reference->parent()->Contains(end)) {
        reference = reference->parent();
      }
      new_node = reference->parent();
      new_offset = NodeIndex(reference) + 1;
    }

    auto process_partial = [&](Node* partial, bool is_first) {
      if (partial->IsCharacterData()) {
        if (is_first) {
          take_text(partial, start_offset, NodeLength(partial), fragment);
        } else {
          take_text(partial, 0, end_offset, fragment);
        }
        return;
      }
      ContainerNode* shell = nullptr;
      if (fragment) {
        shell = static_cast<ContainerNode*>(
            partial->Clone(isolate, document, false));
        fragment->AppendChild(isolate, shell, &exception_state);
      }
      Range* sub = MakeGarbageCollected<Range>(isolate, document);
      if (is_first) {
        sub->start_container = start;
        sub->start_offset = start_offset;
        sub->end_container = partial;
        sub->end_offset = NodeLength(partial);
      } else {
        sub->start_container = partial;
        sub->start_offset = 0;
        sub->end_container = end;
        sub->end_offset = end_offset;
      }
      DocumentFragment* inner =
          fragment ? document->CreateDocumentFragment(isolate) : nullptr;
      sub->ProcessContents(isolate, inner, clone);
      if (shell) shell->AppendChild(isolate, inner, &exception_state);
    };

    if (first_partial) process_partial(first_partial, true);
    for (Node* node : contained) {
      if (clone) {
        fragment->AppendChild(isolate, node->Clone(isolate, document, true),
                              &exception_state);
      } else if (fragment) {
        fragment->AppendChild(isolate, node, &exception_state);
      } else {
        node->Remove(isolate);
      }
    }
    if (last_partial) process_partial(last_partial, false);

    if (!clone) {
      start_container = new_node;
      start_offset = std::min(new_offset, NodeLength(new_node));
      Collapse(true);
    }
  }

  Member<Node> start_container;
  uint32_t start_offset = 0;
  Member<Node> end_container;
  uint32_t end_offset = 0;
};

// The element whose box stands in for a boundary point's position.
Element* ElementFor(Node* node) {
  while (node && !node->IsElement()) node = node->parent();
  return static_cast<Element*>(node);
}

DOMRect* RangeRect(Isolate* isolate, Range* range) {
  Element* element = ElementFor(range->start_container.Get());
  if (!element) return MakeGarbageCollected<DOMRect>(isolate, 0, 0, 0, 0);
  LayoutRect box = BoundingClientRect(element);
  double char_width, line_height;
  FontMetrics(element, &char_width, &line_height);
  if (range->start_container->IsText()) {
    // Within a text node, characters advance by a fixed width on one line.
    uint32_t end = range->start_container == range->end_container
                       ? range->end_offset
                       : NodeLength(range->start_container.Get());
    return MakeGarbageCollected<DOMRect>(
        isolate, box.x + range->start_offset * char_width, box.y,
        (end - range->start_offset) * char_width, line_height);
  }
  if (range->collapsed()) {
    return MakeGarbageCollected<DOMRect>(isolate, box.x, box.y, 0,
                                         std::min(box.height, line_height));
  }
  return MakeGarbageCollected<DOMRect>(isolate, box.x, box.y, box.width,
                                       box.height);
}

bool NodeAndOffset(const CallbackInfo& info, Node** node, uint32_t* offset) {
  if (!RequireArguments(info, 2)) return false;
  *node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!*node) return false;
  double value;
  if (!ToNumber(info, info[1], &value)) return false;
  *offset = value > 0 ? static_cast<uint32_t>(value) : 0;
  return true;
}

template <bool start>
CALLBACK(RangeSetBoundary) {
  Node* node;
  uint32_t offset;
  if (!NodeAndOffset(info, &node, &offset)) return;
  ExceptionState exception_state;
  Receiver<Range>(info)->SetBoundary(start, node, offset, &exception_state);
  ThrowIfFailed(info.GetIsolate(), exception_state);
}

// setStartBefore / setStartAfter / setEndBefore / setEndAfter.
template <bool start, bool before>
CALLBACK(RangeSetRelative) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!node) return;
  ContainerNode* parent = node->parent();
  if (!parent) {
    ThrowDOMException(isolate, "InvalidNodeTypeError",
                      "the given Node has no parent.");
    return;
  }
  ExceptionState exception_state;
  Receiver<Range>(info)->SetBoundary(
      start, parent, NodeIndex(node) + (before ? 0 : 1), &exception_state);
  ThrowIfFailed(isolate, exception_state);
}

template <bool contents>
CALLBACK(RangeSelectNode) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!node) return;
  Range* range = Receiver<Range>(info);
  if (contents) {
    range->start_container = node;
    range->start_offset = 0;
    range->end_container = node;
    range->end_offset = NodeLength(node);
    return;
  }
  ContainerNode* parent = node->parent();
  if (!parent) {
    ThrowDOMException(isolate, "InvalidNodeTypeError",
                      "the given Node has no parent.");
    return;
  }
  uint32_t index = NodeIndex(node);
  range->start_container = parent;
  range->start_offset = index;
  range->end_container = parent;
  range->end_offset = index + 1;
}

template <bool extract, bool clone>
CALLBACK(RangeContents) {
  Isolate* isolate = info.GetIsolate();
  Range* range = Receiver<Range>(info);
  Node* start = range->start_container.Get();
  Document* document =
      start->IsDocument() ? static_cast<Document*>(start) : start->document();
  CEReactionsScope reactions(isolate);
  DocumentFragment* fragment =
      extract || clone ? document->CreateDocumentFragment(isolate) : nullptr;
  range->ProcessContents(isolate, fragment, clone);
  if (fragment) Return(info, fragment);
}

CALLBACK(RangeInsertNode) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!node) return;
  Range* range = Receiver<Range>(info);
  Node* start = range->start_container.Get();
  CEReactionsScope reactions(isolate);
  ExceptionState exception_state;
  ContainerNode* parent;
  Node* reference;
  if (start->IsText()) {
    parent = start->parent();
    if (!parent) {
      ThrowDOMException(isolate, "HierarchyRequestError",
                        "The container has no parent.");
      return;
    }
    // Split the text node at the start offset.
    std::u16string data = Utf16Data(start);
    Node* tail = start->document()->CreateTextNode(
        isolate, FromUtf16(data.substr(range->start_offset)));
    static_cast<CharacterData*>(start)->SetData(
        isolate, FromUtf16(data.substr(0, range->start_offset)));
    parent->InsertBefore(isolate, tail, start->next_sibling(),
                         &exception_state);
    reference = tail;
  } else if (start->IsContainer()) {
    parent = static_cast<ContainerNode*>(start);
    reference = ChildAt(start, range->start_offset);
  } else {
    ThrowDOMException(isolate, "HierarchyRequestError",
                      "Nodes cannot be inserted into this container.");
    return;
  }
  bool was_collapsed = range->collapsed();
  parent->InsertBefore(isolate, node, reference, &exception_state);
  if (ThrowIfFailed(isolate, exception_state)) return;
  if (was_collapsed) {
    range->end_container = parent;
    range->end_offset = reference ? NodeIndex(reference) : NodeLength(parent);
  }
}

CALLBACK(RangeConstructor) {
  if (!RequireConstructCall(info)) return;
  Isolate* isolate = info.GetIsolate();
  WrapThis(info, MakeGarbageCollected<Range>(
                     isolate, Window::Current(isolate)->document()));
}

void InstallRangeBindings(Isolate* isolate) {
  const Interface kRange = Interface::kRange;
  SetConstructor(isolate, kRange, RangeConstructor);
  DefineAccessor(isolate, kRange, "startContainer",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Range>(info)->start_container.Get());
                 });
  DefineAccessor(isolate, kRange, "startOffset", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<Range>(info)->start_offset);
  });
  DefineAccessor(isolate, kRange, "endContainer", [](const CallbackInfo& info) {
    Return(info, Receiver<Range>(info)->end_container.Get());
  });
  DefineAccessor(isolate, kRange, "endOffset", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<Range>(info)->end_offset);
  });
  DefineAccessor(isolate, kRange, "collapsed", [](const CallbackInfo& info) {
    info.GetReturnValue().Set(Receiver<Range>(info)->collapsed());
  });
  DefineAccessor(isolate, kRange, "commonAncestorContainer",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Range>(info)->CommonAncestor());
                 });
  DefineMethod(isolate, kRange, "setStart", RangeSetBoundary<true>, 2);
  DefineMethod(isolate, kRange, "setEnd", RangeSetBoundary<false>, 2);
  DefineMethod(isolate, kRange, "setStartBefore",
               (RangeSetRelative<true, true>), 1);
  DefineMethod(isolate, kRange, "setStartAfter",
               (RangeSetRelative<true, false>), 1);
  DefineMethod(isolate, kRange, "setEndBefore", (RangeSetRelative<false, true>),
               1);
  DefineMethod(isolate, kRange, "setEndAfter", (RangeSetRelative<false, false>),
               1);
  DefineMethod(isolate, kRange, "collapse", [](const CallbackInfo& info) {
    Receiver<Range>(info)->Collapse(info.Length() > 0 &&
                                    info[0]->BooleanValue(info.GetIsolate()));
  });
  DefineMethod(isolate, kRange, "selectNode", RangeSelectNode<false>, 1);
  DefineMethod(isolate, kRange, "selectNodeContents", RangeSelectNode<true>, 1);
  DefineMethod(isolate, kRange, "cloneRange", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Range* range = Receiver<Range>(info);
    Node* start = range->start_container.Get();
    Range* copy = MakeGarbageCollected<Range>(
        isolate, start->IsDocument() ? static_cast<Document*>(start)
                                     : start->document());
    copy->start_container = range->start_container;
    copy->start_offset = range->start_offset;
    copy->end_container = range->end_container;
    copy->end_offset = range->end_offset;
    Return(info, copy);
  });
  DefineMethod(isolate, kRange, "detach", [](const CallbackInfo& info) {});
  DefineMethod(isolate, kRange, "toString", [](const CallbackInfo& info) {
    Return(info, Receiver<Range>(info)->ToString());
  });
  DefineMethod(isolate, kRange, "deleteContents",
               (RangeContents<false, false>));
  DefineMethod(isolate, kRange, "extractContents",
               (RangeContents<true, false>));
  DefineMethod(isolate, kRange, "cloneContents", (RangeContents<false, true>));
  DefineMethod(isolate, kRange, "insertNode", RangeInsertNode, 1);
  DefineMethod(
      isolate, kRange, "compareBoundaryPoints",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 2)) return;
        int how;
        if (!ToInt(info, info[0], &how)) return;
        Range* other = To<Range>(isolate, info[1], Interface::kRange);
        if (!other || how < 0 || how > 3) {
          ThrowTypeError(isolate, "Invalid arguments.");
          return;
        }
        Range* self = Receiver<Range>(info);
        // START_TO_START=0, START_TO_END=1, END_TO_END=2, END_TO_START=3.
        bool this_start = how == 0 || how == 3;
        bool other_start = how == 0 || how == 1;
        info.GetReturnValue().Set(ComparePoints(
            this_start ? self->start_container.Get()
                       : self->end_container.Get(),
            this_start ? self->start_offset : self->end_offset,
            other_start ? other->start_container.Get()
                        : other->end_container.Get(),
            other_start ? other->start_offset : other->end_offset));
      },
      2);
  DefineMethod(
      isolate, kRange, "comparePoint",
      [](const CallbackInfo& info) {
        Node* node;
        uint32_t offset;
        if (!NodeAndOffset(info, &node, &offset)) return;
        Range* range = Receiver<Range>(info);
        if (ComparePoints(node, offset, range->start_container.Get(),
                          range->start_offset) < 0) {
          info.GetReturnValue().Set(-1);
        } else if (ComparePoints(node, offset, range->end_container.Get(),
                                 range->end_offset) > 0) {
          info.GetReturnValue().Set(1);
        } else {
          info.GetReturnValue().Set(0);
        }
      },
      2);
  DefineMethod(
      isolate, kRange, "isPointInRange",
      [](const CallbackInfo& info) {
        Node* node;
        uint32_t offset;
        if (!NodeAndOffset(info, &node, &offset)) return;
        Range* range = Receiver<Range>(info);
        info.GetReturnValue().Set(
            node->Root() == range->start_container->Root() &&
            ComparePoints(node, offset, range->start_container.Get(),
                          range->start_offset) >= 0 &&
            ComparePoints(node, offset, range->end_container.Get(),
                          range->end_offset) <= 0);
      },
      2);
  DefineMethod(
      isolate, kRange, "intersectsNode",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
        if (!node) return;
        Range* range = Receiver<Range>(info);
        ContainerNode* parent = node->parent();
        if (node->Root() != range->start_container->Root()) {
          info.GetReturnValue().Set(false);
          return;
        }
        if (!parent) {
          info.GetReturnValue().Set(true);
          return;
        }
        uint32_t index = NodeIndex(node);
        info.GetReturnValue().Set(
            ComparePoints(parent, index, range->end_container.Get(),
                          range->end_offset) < 0 &&
            ComparePoints(parent, index + 1, range->start_container.Get(),
                          range->start_offset) > 0);
      },
      1);
  DefineMethod(
      isolate, kRange, "createContextualFragment",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        std::string html;
        if (!ToStdString(info, info[0], &html)) return;
        Range* range = Receiver<Range>(info);
        Node* start = range->start_container.Get();
        Document* document = start->IsDocument() ? static_cast<Document*>(start)
                                                 : start->document();
        DocumentFragment* fragment = document->CreateDocumentFragment(isolate);
        ParseHTMLFragment(isolate, fragment, ElementFor(start), html);
        Return(info, fragment);
      },
      1);
  DefineMethod(
      isolate, kRange, "getBoundingClientRect", [](const CallbackInfo& info) {
        Return(info, RangeRect(info.GetIsolate(), Receiver<Range>(info)));
      });
  DefineMethod(isolate, kRange, "getClientRects", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Local<Value> rect =
        ToV8(isolate, RangeRect(isolate, Receiver<Range>(info)));
    info.GetReturnValue().Set(Array::New(isolate, &rect, 1));
  });
  DefineConstant(isolate, kRange, "START_TO_START", 0);
  DefineConstant(isolate, kRange, "START_TO_END", 1);
  DefineConstant(isolate, kRange, "END_TO_END", 2);
  DefineConstant(isolate, kRange, "END_TO_START", 3);
}

// --- Selection ---------------------------------------------------------------

class Selection final : public ScriptWrappable {
 public:
  explicit Selection(Document* document)
      : ScriptWrappable(Interface::kSelection), document_(document) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(document_);
    visitor->Trace(range);
  }
  Window* WrapperWindow() const override { return document_->window(); }
  Document* document() const { return document_.Get(); }

  Node* anchor_node() const {
    if (!range) return nullptr;
    return backwards ? range->end_container.Get()
                     : range->start_container.Get();
  }
  uint32_t anchor_offset() const {
    if (!range) return 0;
    return backwards ? range->end_offset : range->start_offset;
  }
  Node* focus_node() const {
    if (!range) return nullptr;
    return backwards ? range->start_container.Get()
                     : range->end_container.Get();
  }
  uint32_t focus_offset() const {
    if (!range) return 0;
    return backwards ? range->start_offset : range->end_offset;
  }

  // Sets the selection to anchor..focus, which may be in either order.
  void Set(Isolate* isolate, Node* anchor, uint32_t anchor_offset, Node* focus,
           uint32_t focus_offset) {
    Range* updated = MakeGarbageCollected<Range>(isolate, document_.Get());
    backwards = anchor->Root() == focus->Root() &&
                ComparePoints(anchor, anchor_offset, focus, focus_offset) > 0;
    updated->start_container = backwards ? focus : anchor;
    updated->start_offset = backwards ? focus_offset : anchor_offset;
    updated->end_container = backwards ? anchor : focus;
    updated->end_offset = backwards ? anchor_offset : focus_offset;
    range = updated;
    DidChange(isolate);
  }

  // `selectionchange` is fired at the document from a task, once per batch
  // of changes.
  void DidChange(Isolate* isolate) {
    if (change_task_posted_) return;
    change_task_posted_ = true;
    cppgc::Persistent<Selection> self(this);
    PostTask(isolate, [isolate, self] {
      self->change_task_posted_ = false;
      HandleScope handle_scope(isolate);
      Event* event = MakeGarbageCollected<Event>(
          isolate, isolate, Interface::kEvent, AtomicString("selectionchange"));
      event->is_trusted = true;
      self->document_->DispatchEvent(isolate, event);
    });
  }

  Member<Range> range;
  bool backwards = false;

 private:
  Member<Document> document_;
  bool change_task_posted_ = false;
};

Selection* SelectionOf(Isolate* isolate, Document* document) {
  if (!document->selection) {
    document->selection = MakeGarbageCollected<Selection>(isolate, document);
  }
  return static_cast<Selection*>(document->selection.Get());
}

void InstallSelectionBindings(Isolate* isolate) {
  const Interface kSelection = Interface::kSelection;
  DefineAccessor(isolate, kSelection, "anchorNode",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Selection>(info)->anchor_node());
                 });
  DefineAccessor(
      isolate, kSelection, "anchorOffset", [](const CallbackInfo& info) {
        info.GetReturnValue().Set(Receiver<Selection>(info)->anchor_offset());
      });
  DefineAccessor(isolate, kSelection, "focusNode",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Selection>(info)->focus_node());
                 });
  DefineAccessor(
      isolate, kSelection, "focusOffset", [](const CallbackInfo& info) {
        info.GetReturnValue().Set(Receiver<Selection>(info)->focus_offset());
      });
  auto is_collapsed = [](const CallbackInfo& info) {
    Selection* selection = Receiver<Selection>(info);
    info.GetReturnValue().Set(!selection->range ||
                              selection->range->collapsed());
  };
  DefineAccessor(isolate, kSelection, "isCollapsed", is_collapsed);
  DefineAccessor(
      isolate, kSelection, "rangeCount", [](const CallbackInfo& info) {
        info.GetReturnValue().Set(Receiver<Selection>(info)->range ? 1 : 0);
      });
  DefineAccessor(isolate, kSelection, "type", [](const CallbackInfo& info) {
    Selection* selection = Receiver<Selection>(info);
    Return(info, AtomicString(!selection->range               ? "None"
                              : selection->range->collapsed() ? "Caret"
                                                              : "Range"));
  });
  DefineMethod(
      isolate, kSelection, "getRangeAt",
      [](const CallbackInfo& info) {
        Selection* selection = Receiver<Selection>(info);
        int index = 0;
        if (info.Length() > 0 && !ToInt(info, info[0], &index)) return;
        if (index != 0 || !selection->range) {
          ThrowDOMException(info.GetIsolate(), "IndexSizeError",
                            std::to_string(index) + " is not a valid index.");
          return;
        }
        Return(info, selection->range.Get());
      },
      1);
  DefineMethod(
      isolate, kSelection, "addRange",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Range* range = info.Length()
                           ? To<Range>(isolate, info[0], Interface::kRange)
                           : nullptr;
        if (!range) {
          ThrowTypeError(isolate, "parameter 1 is not of type 'Range'.");
          return;
        }
        Selection* selection = Receiver<Selection>(info);
        // Like Blink, only one range is supported; further ones are ignored.
        if (selection->range) return;
        selection->range = range;
        selection->backwards = false;
        selection->DidChange(isolate);
      },
      1);
  auto remove_all = [](const CallbackInfo& info) {
    Selection* selection = Receiver<Selection>(info);
    if (!selection->range) return;
    selection->range = nullptr;
    selection->DidChange(info.GetIsolate());
  };
  DefineMethod(isolate, kSelection, "removeAllRanges", remove_all);
  DefineMethod(isolate, kSelection, "empty", remove_all);
  DefineMethod(isolate, kSelection, "removeRange", remove_all, 1);
  auto collapse = [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Selection* selection = Receiver<Selection>(info);
    if (info.Length() < 1 || info[0]->IsNullOrUndefined()) {
      selection->range = nullptr;
      selection->DidChange(isolate);
      return;
    }
    Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
    if (!node) return;
    double offset = 0;
    if (info.Length() > 1 && !ToNumber(info, info[1], &offset)) return;
    if (offset < 0 || offset > NodeLength(node)) {
      ThrowDOMException(isolate, "IndexSizeError",
                        "The offset is larger than the node's length.");
      return;
    }
    uint32_t point = static_cast<uint32_t>(offset);
    selection->Set(isolate, node, point, node, point);
  };
  DefineMethod(isolate, kSelection, "collapse", collapse, 1);
  DefineMethod(isolate, kSelection, "setPosition", collapse, 1);
  DefineMethod(
      isolate, kSelection, "collapseToStart", [](const CallbackInfo& info) {
        Selection* selection = Receiver<Selection>(info);
        if (!selection->range) return;
        Range* range = selection->range.Get();
        selection->Set(info.GetIsolate(), range->start_container.Get(),
                       range->start_offset, range->start_container.Get(),
                       range->start_offset);
      });
  DefineMethod(isolate, kSelection, "collapseToEnd",
               [](const CallbackInfo& info) {
                 Selection* selection = Receiver<Selection>(info);
                 if (!selection->range) return;
                 Range* range = selection->range.Get();
                 selection->Set(info.GetIsolate(), range->end_container.Get(),
                                range->end_offset, range->end_container.Get(),
                                range->end_offset);
               });
  DefineMethod(
      isolate, kSelection, "extend",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        Selection* selection = Receiver<Selection>(info);
        Node* node;
        uint32_t offset = 0;
        if (!RequireArguments(info, 1)) return;
        node = ToNodeOrThrow(info, info[0], "parameter 1");
        if (!node) return;
        double value = 0;
        if (info.Length() > 1 && !ToNumber(info, info[1], &value)) return;
        offset = value > 0 ? static_cast<uint32_t>(value) : 0;
        if (!selection->range) {
          ThrowDOMException(isolate, "InvalidStateError",
                            "This Selection object doesn't have any Ranges.");
          return;
        }
        selection->Set(isolate, selection->anchor_node(),
                       selection->anchor_offset(), node, offset);
      },
      1);
  DefineMethod(
      isolate, kSelection, "setBaseAndExtent",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 4)) return;
        Node* anchor = ToNodeOrThrow(info, info[0], "parameter 1");
        if (!anchor) return;
        Node* focus = ToNodeOrThrow(info, info[2], "parameter 3");
        if (!focus) return;
        double anchor_offset, focus_offset;
        if (!ToNumber(info, info[1], &anchor_offset) ||
            !ToNumber(info, info[3], &focus_offset)) {
          return;
        }
        if (anchor_offset < 0 || anchor_offset > NodeLength(anchor) ||
            focus_offset < 0 || focus_offset > NodeLength(focus)) {
          ThrowDOMException(isolate, "IndexSizeError",
                            "The offset is larger than the node's length.");
          return;
        }
        Receiver<Selection>(info)->Set(
            isolate, anchor, static_cast<uint32_t>(anchor_offset), focus,
            static_cast<uint32_t>(focus_offset));
      },
      4);
  DefineMethod(
      isolate, kSelection, "selectAllChildren",
      [](const CallbackInfo& info) {
        if (!RequireArguments(info, 1)) return;
        Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
        if (!node) return;
        Receiver<Selection>(info)->Set(info.GetIsolate(), node, 0, node,
                                       NodeLength(node));
      },
      1);
  DefineMethod(
      isolate, kSelection, "containsNode",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        Node* node = ToNodeOrThrow(info, info[0], "parameter 1");
        if (!node) return;
        Selection* selection = Receiver<Selection>(info);
        ContainerNode* parent = node->parent();
        if (!selection->range || !parent ||
            node->Root() != selection->range->start_container->Root()) {
          info.GetReturnValue().Set(false);
          return;
        }
        Range* range = selection->range.Get();
        bool partial = info.Length() > 1 && info[1]->BooleanValue(isolate);
        uint32_t index = NodeIndex(node);
        int start = ComparePoints(parent, index, range->start_container.Get(),
                                  range->start_offset);
        int end = ComparePoints(parent, index + 1, range->end_container.Get(),
                                range->end_offset);
        if (partial) {
          info.GetReturnValue().Set(
              ComparePoints(parent, index, range->end_container.Get(),
                            range->end_offset) < 0 &&
              ComparePoints(parent, index + 1, range->start_container.Get(),
                            range->start_offset) > 0);
        } else {
          info.GetReturnValue().Set(start >= 0 && end <= 0);
        }
      },
      1);
  DefineMethod(isolate, kSelection, "deleteFromDocument",
               [](const CallbackInfo& info) {
                 Isolate* isolate = info.GetIsolate();
                 Selection* selection = Receiver<Selection>(info);
                 if (!selection->range) return;
                 CEReactionsScope reactions(isolate);
                 selection->range->ProcessContents(isolate, nullptr, false);
               });
  DefineMethod(isolate, kSelection, "toString", [](const CallbackInfo& info) {
    Selection* selection = Receiver<Selection>(info);
    Return(info,
           selection->range ? selection->range->ToString() : String::Empty());
  });
  DefineMethod(isolate, kSelection, "modify", [](const CallbackInfo& info) {
    // Moving by character, word or line needs text layout.
    Receiver<Selection>(info)->document()->window()->ReportUnsupported(
        info.GetIsolate(), "Selection.modify()");
  });
}

// --- TreeWalker and NodeIterator ---------------------------------------------

constexpr int kFilterAccept = 1;
constexpr int kFilterReject = 2;
constexpr int kFilterSkip = 3;

class Traversal final : public ScriptWrappable {
 public:
  Traversal(Isolate* isolate, Interface interface, Node* root,
            uint32_t what_to_show, Local<Value> filter)
      : ScriptWrappable(interface),
        root(root),
        current(root),
        what_to_show(what_to_show) {
    if (!filter.IsEmpty() && filter->IsObject()) {
      this->filter.Reset(isolate, filter);
    }
  }
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(root);
    visitor->Trace(current);
    visitor->Trace(filter);
  }
  Window* WrapperWindow() const override { return root->WrapperWindow(); }

  // Runs whatToShow and the filter. Returns 0 if the filter threw.
  int Accept(Isolate* isolate, Node* node) {
    if (!((what_to_show >> (node->node_type() - 1)) & 1)) return kFilterSkip;
    if (filter.IsEmpty()) return kFilterAccept;
    Local<Context> context = isolate->GetCurrentContext();
    Local<Value> callback = filter.Get(isolate);
    Local<Value> receiver = Undefined(isolate);
    Local<Function> function;
    if (callback->IsFunction()) {
      function = callback.As<Function>();
    } else {
      Local<Value> accept_node;
      if (!callback.As<Object>()
               ->Get(context, V8Name(isolate, "acceptNode"))
               .ToLocal(&accept_node)) {
        return 0;
      }
      if (!accept_node->IsFunction()) {
        ThrowTypeError(isolate, "The NodeFilter's acceptNode is not callable.");
        return 0;
      }
      function = accept_node.As<Function>();
      receiver = callback;
    }
    Local<Value> args[] = {ToV8(isolate, node)};
    Local<Value> result;
    int32_t verdict;
    if (!function->Call(context, receiver, 1, args).ToLocal(&result) ||
        !result->Int32Value(context).To(&verdict)) {
      return 0;
    }
    return verdict;
  }

  Member<Node> root;
  Member<Node> current;
  uint32_t what_to_show;
  TracedReference<Value> filter;
  // NodeIterator only: whether the pointer is before the reference node.
  bool pointer_before_reference = true;
};

template <Interface interface>
CALLBACK(CreateTraversal) {
  Isolate* isolate = info.GetIsolate();
  if (!RequireArguments(info, 1)) return;
  Node* root = ToNodeOrThrow(info, info[0], "parameter 1");
  if (!root) return;
  double what_to_show = 0xFFFFFFFF;
  if (info.Length() > 1 && !info[1]->IsUndefined() &&
      !ToNumber(info, info[1], &what_to_show)) {
    return;
  }
  Local<Value> filter;
  if (info.Length() > 2) filter = info[2];
  Return(info, MakeGarbageCollected<Traversal>(
                   isolate, isolate, interface, root,
                   static_cast<uint32_t>(what_to_show), filter));
}

// TreeWalker.nextNode()
CALLBACK(TreeWalkerNextNode) {
  Isolate* isolate = info.GetIsolate();
  Traversal* walker = Receiver<Traversal>(info);
  Node* node = walker->current.Get();
  int result = kFilterAccept;
  while (true) {
    while (result != kFilterReject && node->first_child()) {
      node = node->first_child();
      result = walker->Accept(isolate, node);
      if (result == 0) return;
      if (result == kFilterAccept) {
        walker->current = node;
        Return(info, node);
        return;
      }
    }
    Node* sibling = nullptr;
    Node* temporary = node;
    while (temporary) {
      if (temporary == walker->root) {
        info.GetReturnValue().SetNull();
        return;
      }
      sibling = temporary->next_sibling();
      if (sibling) break;
      temporary = temporary->parent();
    }
    if (!sibling) {
      info.GetReturnValue().SetNull();
      return;
    }
    node = sibling;
    result = walker->Accept(isolate, node);
    if (result == 0) return;
    if (result == kFilterAccept) {
      walker->current = node;
      Return(info, node);
      return;
    }
  }
}

CALLBACK(TreeWalkerPreviousNode) {
  Isolate* isolate = info.GetIsolate();
  Traversal* walker = Receiver<Traversal>(info);
  Node* node = walker->current.Get();
  while (node != walker->root) {
    Node* sibling = node->previous_sibling();
    while (sibling) {
      node = sibling;
      int result = walker->Accept(isolate, node);
      if (result == 0) return;
      while (result != kFilterReject && node->last_child()) {
        node = node->last_child();
        result = walker->Accept(isolate, node);
        if (result == 0) return;
      }
      if (result == kFilterAccept) {
        walker->current = node;
        Return(info, node);
        return;
      }
      sibling = node->previous_sibling();
    }
    if (node == walker->root || !node->parent()) break;
    node = node->parent();
    int result = walker->Accept(isolate, node);
    if (result == 0) return;
    if (result == kFilterAccept) {
      walker->current = node;
      Return(info, node);
      return;
    }
  }
  info.GetReturnValue().SetNull();
}

CALLBACK(TreeWalkerParentNode) {
  Isolate* isolate = info.GetIsolate();
  Traversal* walker = Receiver<Traversal>(info);
  Node* node = walker->current.Get();
  while (node && node != walker->root) {
    node = node->parent();
    if (!node) break;
    int result = walker->Accept(isolate, node);
    if (result == 0) return;
    if (result == kFilterAccept) {
      walker->current = node;
      Return(info, node);
      return;
    }
  }
  info.GetReturnValue().SetNull();
}

// firstChild / lastChild.
template <bool first>
CALLBACK(TreeWalkerChild) {
  Isolate* isolate = info.GetIsolate();
  Traversal* walker = Receiver<Traversal>(info);
  Node* node =
      first ? walker->current->first_child() : walker->current->last_child();
  while (node) {
    int result = walker->Accept(isolate, node);
    if (result == 0) return;
    if (result == kFilterAccept) {
      walker->current = node;
      Return(info, node);
      return;
    }
    if (result == kFilterSkip) {
      Node* child = first ? node->first_child() : node->last_child();
      if (child) {
        node = child;
        continue;
      }
    }
    while (node) {
      Node* sibling = first ? node->next_sibling() : node->previous_sibling();
      if (sibling) {
        node = sibling;
        break;
      }
      Node* parent = node->parent();
      if (!parent || parent == walker->root || parent == walker->current) {
        info.GetReturnValue().SetNull();
        return;
      }
      node = parent;
    }
  }
  info.GetReturnValue().SetNull();
}

// nextSibling / previousSibling.
template <bool next>
CALLBACK(TreeWalkerSibling) {
  Isolate* isolate = info.GetIsolate();
  Traversal* walker = Receiver<Traversal>(info);
  Node* node = walker->current.Get();
  if (node == walker->root) {
    info.GetReturnValue().SetNull();
    return;
  }
  while (true) {
    Node* sibling = next ? node->next_sibling() : node->previous_sibling();
    while (sibling) {
      node = sibling;
      int result = walker->Accept(isolate, node);
      if (result == 0) return;
      if (result == kFilterAccept) {
        walker->current = node;
        Return(info, node);
        return;
      }
      sibling = next ? node->first_child() : node->last_child();
      if (result == kFilterReject || !sibling) {
        sibling = next ? node->next_sibling() : node->previous_sibling();
      }
    }
    node = node->parent();
    if (!node || node == walker->root) break;
    int result = walker->Accept(isolate, node);
    if (result == 0) return;
    if (result == kFilterAccept) break;
  }
  info.GetReturnValue().SetNull();
}

// NodeIterator.nextNode() / previousNode().
template <bool next>
CALLBACK(NodeIteratorTraverse) {
  Isolate* isolate = info.GetIsolate();
  Traversal* iterator = Receiver<Traversal>(info);
  Node* node = iterator->current.Get();
  bool before = iterator->pointer_before_reference;
  while (true) {
    if (next) {
      if (!before) {
        node = node->NextInPreOrder(iterator->root.Get());
        if (!node) {
          info.GetReturnValue().SetNull();
          return;
        }
      } else {
        before = false;
      }
    } else {
      if (before) {
        if (node == iterator->root) {
          info.GetReturnValue().SetNull();
          return;
        }
        // Previous node in tree order.
        if (Node* sibling = node->previous_sibling()) {
          node = sibling;
          while (node->last_child()) node = node->last_child();
        } else {
          node = node->parent();
        }
      } else {
        before = true;
      }
    }
    int result = iterator->Accept(isolate, node);
    if (result == 0) return;
    if (result == kFilterAccept) break;
  }
  iterator->current = node;
  iterator->pointer_before_reference = before;
  Return(info, node);
}

void InstallTraversalBindings(Isolate* isolate) {
  for (Interface interface :
       {Interface::kTreeWalker, Interface::kNodeIterator}) {
    DefineAccessor(isolate, interface, "root", [](const CallbackInfo& info) {
      Return(info, Receiver<Traversal>(info)->root.Get());
    });
    DefineAccessor(
        isolate, interface, "whatToShow", [](const CallbackInfo& info) {
          info.GetReturnValue().Set(Receiver<Traversal>(info)->what_to_show);
        });
    DefineAccessor(isolate, interface, "filter", [](const CallbackInfo& info) {
      Traversal* traversal = Receiver<Traversal>(info);
      if (traversal->filter.IsEmpty()) {
        info.GetReturnValue().SetNull();
      } else {
        info.GetReturnValue().Set(traversal->filter.Get(info.GetIsolate()));
      }
    });
  }
  const Interface kWalker = Interface::kTreeWalker;
  DefineAccessor(
      isolate, kWalker, "currentNode",
      [](const CallbackInfo& info) {
        Return(info, Receiver<Traversal>(info)->current.Get());
      },
      [](const CallbackInfo& info) {
        Node* node = ToNodeOrThrow(info, info[0], "The provided value");
        if (node) Receiver<Traversal>(info)->current = node;
      });
  DefineMethod(isolate, kWalker, "nextNode", TreeWalkerNextNode);
  DefineMethod(isolate, kWalker, "previousNode", TreeWalkerPreviousNode);
  DefineMethod(isolate, kWalker, "parentNode", TreeWalkerParentNode);
  DefineMethod(isolate, kWalker, "firstChild", TreeWalkerChild<true>);
  DefineMethod(isolate, kWalker, "lastChild", TreeWalkerChild<false>);
  DefineMethod(isolate, kWalker, "nextSibling", TreeWalkerSibling<true>);
  DefineMethod(isolate, kWalker, "previousSibling", TreeWalkerSibling<false>);

  const Interface kIterator = Interface::kNodeIterator;
  DefineAccessor(isolate, kIterator, "referenceNode",
                 [](const CallbackInfo& info) {
                   Return(info, Receiver<Traversal>(info)->current.Get());
                 });
  DefineAccessor(isolate, kIterator, "pointerBeforeReferenceNode",
                 [](const CallbackInfo& info) {
                   info.GetReturnValue().Set(
                       Receiver<Traversal>(info)->pointer_before_reference);
                 });
  DefineMethod(isolate, kIterator, "nextNode", NodeIteratorTraverse<true>);
  DefineMethod(isolate, kIterator, "previousNode", NodeIteratorTraverse<false>);
  DefineMethod(isolate, kIterator, "detach", [](const CallbackInfo& info) {});

  DefineMethod(isolate, Interface::kDocument, "createTreeWalker",
               CreateTraversal<Interface::kTreeWalker>, 1);
  DefineMethod(isolate, Interface::kDocument, "createNodeIterator",
               CreateTraversal<Interface::kNodeIterator>, 1);
}

// --- DOMImplementation -------------------------------------------------------

class DOMImplementation final : public ScriptWrappable {
 public:
  explicit DOMImplementation(Document* document)
      : ScriptWrappable(Interface::kDOMImplementation), document(document) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(document);
  }
  Window* WrapperWindow() const override { return document->window(); }
  Member<Document> document;
};

void InstallDocumentExtras(Isolate* isolate) {
  const Interface kDocument = Interface::kDocument;
  DefineMethod(isolate, kDocument, "createRange", [](const CallbackInfo& info) {
    Return(info, MakeGarbageCollected<Range>(info.GetIsolate(),
                                             Receiver<Document>(info)));
  });
  DefineMethod(
      isolate, kDocument, "getSelection", [](const CallbackInfo& info) {
        Return(info, SelectionOf(info.GetIsolate(), Receiver<Document>(info)));
      });
  DefineMethod(isolate, Interface::kShadowRoot, "getSelection",
               [](const CallbackInfo& info) {
                 Return(info,
                        SelectionOf(info.GetIsolate(),
                                    Receiver<ShadowRoot>(info)->document()));
               });
  DefineGlobalMethod(isolate, "getSelection", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    Return(info, SelectionOf(isolate, Window::Current(isolate)->document()));
  });
  DefineAccessor(
      isolate, kDocument, "implementation", [](const CallbackInfo& info) {
        // A fresh object per access is not observable beyond
        // identity, which pages do not rely on.
        Return(info, MakeGarbageCollected<DOMImplementation>(
                         info.GetIsolate(), Receiver<Document>(info)));
      });
  const Interface kImplementation = Interface::kDOMImplementation;
  DefineMethod(
      isolate, kImplementation, "createHTMLDocument",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        DOMImplementation* implementation = Receiver<DOMImplementation>(info);
        Document* document = MakeGarbageCollected<Document>(
            isolate, isolate, implementation->document->window());
        std::string title;
        bool has_title = info.Length() > 0 && !info[0]->IsUndefined();
        if (has_title && !ToStdString(info, info[0], &title)) return;
        ParseHTMLDocument(isolate, document, "<!DOCTYPE html>");
        if (has_title) {
          Element* element = document->CreateElement(isolate, Namespace::kHTML,
                                                     AtomicString("title"));
          element->SetTextContent(isolate, String::FromUtf8(title));
          ExceptionState exception_state;
          document->head()->AppendChild(isolate, element, &exception_state);
        }
        document->ready_state = AtomicString("complete");
        Return(info, document);
      });
  DefineMethod(
      isolate, kImplementation, "hasFeature",
      [](const CallbackInfo& info) { info.GetReturnValue().Set(true); });
}

#undef CALLBACK

}  // namespace

void InstallRangeAndTraversalBindings(Isolate* isolate) {
  InstallRangeBindings(isolate);
  InstallSelectionBindings(isolate);
  InstallTraversalBindings(isolate);
  InstallDocumentExtras(isolate);
}

void InitializeRealmForTraversal(Isolate* isolate, Local<Context> context) {
  static const struct {
    const char* name;
    double value;
  } kConstants[] = {
      {"FILTER_ACCEPT", 1},
      {"FILTER_REJECT", 2},
      {"FILTER_SKIP", 3},
      {"SHOW_ALL", 0xFFFFFFFF},
      {"SHOW_ELEMENT", 0x1},
      {"SHOW_ATTRIBUTE", 0x2},
      {"SHOW_TEXT", 0x4},
      {"SHOW_CDATA_SECTION", 0x8},
      {"SHOW_PROCESSING_INSTRUCTION", 0x40},
      {"SHOW_COMMENT", 0x80},
      {"SHOW_DOCUMENT", 0x100},
      {"SHOW_DOCUMENT_TYPE", 0x200},
      {"SHOW_DOCUMENT_FRAGMENT", 0x400},
  };
  Local<Object> node_filter = Object::New(isolate);
  for (const auto& constant : kConstants) {
    USE(node_filter->Set(context, V8Name(isolate, constant.name),
                         Number::New(isolate, constant.value)));
  }
  USE(context->Global()->Set(context, V8Name(isolate, "NodeFilter"),
                             node_filter));
}

}  // namespace mock_dom
}  // namespace v8
