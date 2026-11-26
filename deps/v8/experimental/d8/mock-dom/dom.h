// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The C++ object model of d8's mock DOM (--experimental-mock-dom). It is a
// deliberately small DOM without style resolution, layout or painting: just
// enough tree, event and string machinery that the V8-facing behaviour of real
// web frameworks (API callbacks, wrapper creation, unified-heap tracing, string
// externalization, listener invocation) has the same shape as in a browser.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_DOM_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_DOM_H_

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "experimental/d8/mock-dom/interfaces.h"
#include "experimental/d8/mock-dom/strings.h"
#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/macros.h"
#include "include/cppgc/member.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/visitor.h"
#include "include/v8-context.h"
#include "include/v8-cppgc.h"
#include "include/v8-function.h"
#include "include/v8-object.h"
#include "include/v8-traced-handle.h"

namespace v8 {
namespace mock_dom {

class AdoptedStyleSheets;
class CSSStyleDeclaration;
class CanvasContext;
class ContainerNode;
class CustomElementDefinition;
class CustomElementRegistry;
class DOMStringMap;
class DOMTokenList;
class Document;
class DocumentFragment;
class Element;
class Event;
class History;
class Location;
class MutationObserver;
class MutationObserverRegistration;
class NamedNodeMap;
class Node;
class NodeList;
class SelectorList;
class ShadowRoot;
class Storage;
class Window;

using cppgc::Member;
using cppgc::Visitor;

// Marks a short-lived local vector of raw node pointers. Oilpan cannot see
// into a std::vector's buffer, so each use must be one where every element is
// kept alive by something else for as long as the vector exists: the tree
// the nodes are attached to, or a traced owner such as Event::path.
#define MOCK_DOM_LOCAL_NODES                               \
  CPPGC_PLUGIN_IGNORE(                                     \
      "local vector of nodes kept alive by the tree or a " \
      "traced owner")

// Marks a std container that holds traced references. Oilpan's own rules
// forbid these because a concurrent marker could race with the container
// being resized. They are safe here as long as the owner's Trace() traces
// every element and starts with DeferTraceIfConcurrent(), so that the
// container is only ever walked on the mutator thread.
#define MOCK_DOM_TRACED_CONTAINER                           \
  CPPGC_PLUGIN_IGNORE(                                      \
      "traced std container; the owner defers its tracing " \
      "to the mutator thread")

// Returns true if |object| is being traced by a concurrent marker, in which
// case the object has been queued for tracing on the mutator thread and the
// caller must return without tracing anything.
template <typename T>
bool DeferTraceIfConcurrent(Visitor* visitor, const T* object) {
  return visitor->DeferTraceToMutatorThreadIfConcurrent(
      object,
      [](Visitor* visitor, const void* self) {
        static_cast<const T*>(self)->Trace(visitor);
      },
      sizeof(T));
}

template <typename T, typename... Args>
T* MakeGarbageCollected(Isolate* isolate, Args&&... args) {
  return cppgc::MakeGarbageCollected<T>(
      isolate->GetCppHeap()->GetAllocationHandle(),
      std::forward<Args>(args)...);
}

// Records a DOM exception raised by the object model. The bindings turn it
// into a thrown DOMException.
struct ExceptionState {
  bool failed = false;
  const char* name = "";
  std::string message;
  bool Throw(const char* exception_name, std::string exception_message) {
    failed = true;
    name = exception_name;
    message = std::move(exception_message);
    return false;
  }
};

// Base class of everything that can have a JS wrapper.
class ScriptWrappable : public Object::Wrappable {
 public:
  explicit ScriptWrappable(Interface interface) : interface_(interface) {}
  Interface interface() const { return interface_; }
  void set_interface(Interface interface) { interface_ = interface; }
  bool Is(Interface interface) const {
    return InterfaceInheritsFrom(interface_, interface);
  }
  const char* GetHumanReadableName() const override {
    return InterfaceName(interface_);
  }
  void Trace(Visitor* visitor) const override {
    Object::Wrappable::Trace(visitor);
    visitor->Trace(wrapper_);
  }

  bool has_wrapper() const { return !wrapper_.IsEmpty(); }
  Local<Object> wrapper(Isolate* isolate) const {
    return wrapper_.Get(isolate);
  }
  void set_wrapper(Isolate* isolate, Local<Object> wrapper) {
    wrapper_.Reset(isolate, wrapper);
  }
  // The window whose realm new wrappers for this object are created in, or
  // null to use the current realm.
  virtual Window* WrapperWindow() const { return nullptr; }

 private:
  Interface interface_;
  TracedReference<Object> wrapper_;
};

// --- Events ------------------------------------------------------------------

class EventListener final : public cppgc::GarbageCollected<EventListener> {
 public:
  EventListener(Isolate* isolate, AtomicString type, Local<Value> callback,
                bool capture)
      : type(std::move(type)), capture(capture) {
    this->callback.Reset(isolate, callback);
  }
  void Trace(Visitor* visitor) const {
    visitor->Trace(callback);
    visitor->Trace(next);
  }

  AtomicString type;
  TracedReference<Value> callback;
  bool capture;
  bool once = false;
  bool passive = false;
  // Set for listeners installed through an `onfoo` IDL attribute.
  bool is_attribute = false;
  bool removed = false;
  Member<EventListener> next;
};

class EventTarget : public ScriptWrappable {
 public:
  explicit EventTarget(Interface interface) : ScriptWrappable(interface) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(listeners_);
  }

  virtual Node* AsNode() { return nullptr; }
  virtual Window* AsWindow() { return nullptr; }

  void AddEventListener(Isolate* isolate, const AtomicString& type,
                        Local<Value> callback, bool capture, bool once,
                        bool passive);
  void RemoveEventListener(Isolate* isolate, const AtomicString& type,
                           Local<Value> callback, bool capture);
  // `onfoo` IDL attributes.
  Local<Value> GetAttributeListener(Isolate* isolate, const AtomicString& type);
  void SetAttributeListener(Isolate* isolate, const AtomicString& type,
                            Local<Value> callback);
  bool HasListeners(const AtomicString& type) const;
  EventListener* first_listener() const { return listeners_; }

  // Dispatches |event| at this target. Returns false if the event was
  // cancelled. Listener exceptions are reported to the window, never
  // swallowed.
  bool DispatchEvent(Isolate* isolate, Event* event);

  // Runs the listeners registered on this target for the current phase of
  // |event|.
  void FireListeners(Isolate* isolate, Event* event, bool capture_phase,
                     bool at_target);

 private:
  Member<EventListener> listeners_;
};

// Describes one init-dictionary member / attribute of an event interface.
struct EventField {
  enum Kind : uint8_t { kNumber, kBool, kString, kAny, kTarget };
  const char* name;
  Kind kind;
};

class Event : public ScriptWrappable {
 public:
  enum Phase : uint8_t {
    kNone = 0,
    kCapturing = 1,
    kAtTarget = 2,
    kBubbling = 3
  };

  Event(Isolate* isolate, Interface interface, AtomicString type);
  void Trace(Visitor* visitor) const override;

  const AtomicString& type() const { return type_; }
  void set_type(AtomicString type) { type_ = std::move(type); }

  bool bubbles = false;
  bool cancelable = false;
  bool composed = false;
  bool default_prevented = false;
  bool propagation_stopped = false;
  bool immediate_propagation_stopped = false;
  bool dispatching = false;
  bool in_passive_listener = false;
  bool is_trusted = false;
  Phase phase = kNone;
  double time_stamp = 0;
  Member<EventTarget> target;
  Member<EventTarget> current_target;
  // The propagation path, innermost target first. Kept for composedPath().
  Member<NodeList> path;

  // Per-interface fields, indexed by position in EventFieldsFor(interface()).
  struct Slot {
    double number = 0;
    String string;
  };
  std::vector<Slot> slots;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<TracedReference<Value>> any_slots;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<Member<EventTarget>> target_slots;

  double number(const char* name) const;
  void set_number(const char* name, double value);
  String string(const char* name) const;
  void set_string(const char* name, String value);
  int FieldIndex(const char* name) const;

 private:
  AtomicString type_;
};

// Returns the fields of |interface| including inherited ones, base first.
const std::vector<EventField>& EventFieldsFor(Interface interface);

// --- Nodes -------------------------------------------------------------------

class Node : public EventTarget {
 public:
  enum Type : uint8_t {
    kElementNode = 1,
    kTextNode = 3,
    kCommentNode = 8,
    kDocumentNode = 9,
    kDocumentTypeNode = 10,
    kDocumentFragmentNode = 11,
  };
  enum Flags : uint16_t {
    kIsContainer = 1 << 0,
    kIsConnected = 1 << 1,
    kIsShadowRoot = 1 << 2,
    kIsCustom = 1 << 3,           // custom element, upgraded
    kIsUndefinedCustom = 1 << 4,  // valid custom name, not (yet) upgraded
    kHasRareData = 1 << 5,
    kIsTemplateContent = 1 << 6,
    kHasObservers = 1 << 7,
  };

  Node(Document* document, Interface interface, Type type, uint16_t flags);
  void Trace(Visitor* visitor) const override;

  Node* AsNode() override { return this; }
  Window* WrapperWindow() const override;

  Type node_type() const { return type_; }
  bool IsElement() const { return type_ == kElementNode; }
  bool IsText() const { return type_ == kTextNode; }
  bool IsCharacterData() const {
    return type_ == kTextNode || type_ == kCommentNode;
  }
  bool IsDocument() const { return type_ == kDocumentNode; }
  bool IsFragment() const { return type_ == kDocumentFragmentNode; }
  bool IsContainer() const { return flags_ & kIsContainer; }
  bool IsShadowRoot() const { return flags_ & kIsShadowRoot; }
  bool is_connected() const { return flags_ & kIsConnected; }
  bool has_flag(Flags flag) const { return flags_ & flag; }
  void set_flag(Flags flag, bool value) {
    flags_ = value ? (flags_ | flag) : (flags_ & ~flag);
  }

  Document* document() const { return document_.Get(); }
  ContainerNode* parent() const { return parent_.Get(); }
  Element* parent_element() const;
  Node* previous_sibling() const { return prev_.Get(); }
  Node* next_sibling() const { return next_.Get(); }
  Node* first_child() const;
  Node* last_child() const;
  Element* previous_element_sibling() const;
  Element* next_element_sibling() const;

  // The root of the tree this node is in, not crossing shadow boundaries.
  Node* Root();
  // The shadow root this node is inside of, if any.
  ShadowRoot* ContainingShadowRoot();
  bool Contains(const Node* other) const;
  // Like Contains() but also looks through shadow hosts.
  bool ShadowIncludingContains(const Node* other) const;

  // Pre-order traversal helpers. |stay_within| bounds the walk.
  Node* NextInPreOrder(const Node* stay_within) const;
  Node* NextSkippingChildren(const Node* stay_within) const;

  String NodeName() const;
  String TextContent() const;
  void AppendTextContent(std::string* utf8) const;
  void SetTextContent(Isolate* isolate, const String& text);
  String NodeValue() const;

  Node* Clone(Isolate* isolate, Document* document, bool deep) const;
  void Remove(Isolate* isolate);
  uint16_t CompareDocumentPosition(const Node* other) const;
  bool IsEqualNode(const Node* other) const;
  void Normalize(Isolate* isolate);

  // Node-specific dispatch: builds the propagation path through ancestors,
  // shadow hosts and the window.
  bool DispatchNodeEvent(Isolate* isolate, Event* event);

  MutationObserverRegistration* observer_registrations() const;
  void set_observer_registrations(MutationObserverRegistration* head);
  // Moves this single node (not its subtree) to |document|.
  void AdoptInto(Document* document) { document_ = document; }

 protected:
  friend class ContainerNode;
  friend class Document;
  void set_document(Document* document) { document_ = document; }

  Type type_;
  uint16_t flags_;
  Member<Document> document_;
  Member<ContainerNode> parent_;
  Member<Node> prev_;
  Member<Node> next_;
  Member<MutationObserverRegistration> observers_;
};

class ContainerNode : public Node {
 public:
  ContainerNode(Document* document, Interface interface, Type type,
                uint16_t flags)
      : Node(document, interface, type, flags | kIsContainer) {}
  void Trace(Visitor* visitor) const override;

  Node* first() const { return first_.Get(); }
  Node* last() const { return last_.Get(); }
  Element* first_element_child() const;
  Element* last_element_child() const;
  uint32_t CountChildren() const;
  uint32_t CountElementChildren() const;

  // The DOM "pre-insert", "pre-remove" and "replace" algorithms, including
  // validity checks.
  bool InsertBefore(Isolate* isolate, Node* node, Node* child,
                    ExceptionState* exception_state);
  bool AppendChild(Isolate* isolate, Node* node,
                   ExceptionState* exception_state) {
    return InsertBefore(isolate, node, nullptr, exception_state);
  }
  bool RemoveChild(Isolate* isolate, Node* child,
                   ExceptionState* exception_state);
  bool ReplaceChild(Isolate* isolate, Node* node, Node* child,
                    ExceptionState* exception_state);
  void RemoveChildren(Isolate* isolate);
  // Parser fast path: appends without validity checks or mutation records.
  void ParserAppendChild(Isolate* isolate, Node* node);
  // Parser only: appends |node|, first detaching it from its current parent.
  void ParserMoveChild(Isolate* isolate, Node* node);

  Element* QuerySelector(Isolate* isolate, const String& selectors,
                         ExceptionState* exception_state);
  void QuerySelectorAll(Isolate* isolate, const String& selectors,
                        std::vector<Element*>* out,
                        ExceptionState* exception_state);
  // Finds a descendant by id by walking the subtree. Used for trees that are
  // not covered by the document's id map.
  Element* FindElementById(const AtomicString& id) const;

  // Cached live collections, so that node.childNodes and node.children
  // return the same object every time.
  Member<NodeList> child_nodes_list;
  Member<NodeList> children_list;

 private:
  bool EnsurePreInsertionValidity(Node* node, Node* child,
                                  ExceptionState* exception_state) const;
  void InsertInternal(Isolate* isolate, Node* node, Node* child);
  void RemoveInternal(Isolate* isolate, Node* child);

  Member<Node> first_;
  Member<Node> last_;
};

class CharacterData : public Node {
 public:
  CharacterData(Document* document, Interface interface, Type type, String data)
      : Node(document, interface, type, 0), data_(std::move(data)) {}
  const String& data() const { return data_; }
  void SetData(Isolate* isolate, const String& data);

 private:
  String data_;
};

class DocumentType final : public Node {
 public:
  DocumentType(Document* document, AtomicString name)
      : Node(document, Interface::kDocumentType, kDocumentTypeNode, 0),
        name(std::move(name)) {}
  AtomicString name;
};

class DocumentFragment : public ContainerNode {
 public:
  explicit DocumentFragment(Document* document,
                            Interface interface = Interface::kDocumentFragment,
                            uint16_t flags = 0)
      : ContainerNode(document, interface, kDocumentFragmentNode, flags) {}
};

class ShadowRoot final : public DocumentFragment {
 public:
  ShadowRoot(Document* document, Element* host, bool open)
      : DocumentFragment(document, Interface::kShadowRoot, kIsShadowRoot),
        host_(host),
        open_(open) {}
  void Trace(Visitor* visitor) const override;
  Element* host() const { return host_.Get(); }
  bool open() const { return open_; }
  Member<AdoptedStyleSheets> adopted_style_sheets;

 private:
  Member<Element> host_;
  bool open_;
};

enum class Namespace : uint8_t { kHTML, kSVG, kMathML, kNone };
const char* NamespaceURI(Namespace ns);
Namespace NamespaceFromURI(const String& uri);

struct Attribute {
  AtomicString name;
  AtomicString value;
};

// State that only some elements need.
class ElementRareData final : public cppgc::GarbageCollected<ElementRareData> {
 public:
  void Trace(Visitor* visitor) const;

  Member<ShadowRoot> shadow_root;
  Member<DOMTokenList> class_list;
  Member<CSSStyleDeclaration> style;
  Member<DOMStringMap> dataset;
  Member<NamedNodeMap> attributes;
  Member<CustomElementDefinition> definition;
  Member<DocumentFragment> template_content;
  Member<CanvasContext> canvas_context;
  // Form control state (input, textarea, select, option).
  String value;
  bool has_dirty_value = false;
  bool checked = false;
  bool has_dirty_checked = false;
  bool selected = false;
  bool has_dirty_selected = false;
  bool indeterminate = false;
  double scroll_top = 0;
  double scroll_left = 0;
  // <style> and <link>: the CSSStyleSheet and the text it was built from.
  // For links the loader supplies the text.
  Member<ScriptWrappable> sheet;
  String sheet_source;
  String link_sheet_text;
  // HTMLIFrameElement: the window of the nested realm, set by the loader.
  Member<Window> content_window;
};

class Element : public ContainerNode {
 public:
  Element(Document* document, Interface interface, Namespace ns,
          AtomicString local_name);
  void Trace(Visitor* visitor) const override;

  Namespace ns() const { return ns_; }
  bool IsHTML() const { return ns_ == Namespace::kHTML; }
  const AtomicString& local_name() const { return local_name_; }
  bool HasTag(const char* ascii_lowercase) const {
    return ns_ == Namespace::kHTML && local_name_.EqualsAscii(ascii_lowercase);
  }
  // Upper-cased for HTML elements.
  AtomicString TagName() const;

  // Callers that enumerate attributes must call SynchronizeAttributes()
  // first so that a pending inline-style change is reflected.
  const std::vector<Attribute>& attributes() const { return attributes_; }
  void SynchronizeAttributes() const;
  const AtomicString& GetAttribute(const AtomicString& name) const;
  bool HasAttribute(const AtomicString& name) const;
  void SetAttribute(Isolate* isolate, const AtomicString& name,
                    const AtomicString& value);
  void RemoveAttribute(Isolate* isolate, const AtomicString& name);
  // Parser fast path: no reactions, no mutation records.
  void ParserSetAttribute(const AtomicString& name, const AtomicString& value);
  // Name normalisation for the non-NS attribute API on HTML elements.
  AtomicString NormalizeAttributeName(Isolate* isolate,
                                      Local<v8::String> name) const;

  const AtomicString& id() const { return id_; }
  const std::vector<AtomicString>& class_names() const { return class_names_; }
  bool HasClass(const AtomicString& name) const;

  ElementRareData* rare_data() const { return rare_data_.Get(); }
  ElementRareData* EnsureRareData(Isolate* isolate);
  ShadowRoot* shadow_root() const {
    return rare_data_ ? rare_data_->shadow_root.Get() : nullptr;
  }
  ShadowRoot* AttachShadow(Isolate* isolate, bool open);
  CustomElementDefinition* definition() const {
    return rare_data_ ? rare_data_->definition.Get() : nullptr;
  }
  DocumentFragment* TemplateContent(Isolate* isolate);

  bool Matches(Isolate* isolate, const String& selectors,
               ExceptionState* exception_state);
  Element* Closest(Isolate* isolate, const String& selectors,
                   ExceptionState* exception_state);

  String InnerHTML() const;
  String OuterHTML() const;
  void SetInnerHTML(Isolate* isolate, const String& html);

  // Must be called when state that selectors can test changes without an
  // attribute changing (checkedness, focus, custom element state...).
  void DidChangeState();

  // Form controls.
  bool IsFormControl() const;
  bool IsDisabled() const;
  bool IsCheckable() const;  // checkbox or radio
  bool Checked() const;
  void SetChecked(Isolate* isolate, bool checked);
  String Value() const;
  void SetValue(Isolate* isolate, const String& value);
  bool Selected() const;

  // Focus and activation behaviour.
  void Focus(Isolate* isolate);
  void Blur(Isolate* isolate);
  void Click(Isolate* isolate);

 private:
  void AttributeChanged(Isolate* isolate, const AtomicString& name,
                        const AtomicString& old_value,
                        const AtomicString& new_value, bool from_parser);
  void UpdateClassNames(const AtomicString& value);

  Namespace ns_;
  AtomicString local_name_;
  AtomicString id_;
  std::vector<Attribute> attributes_;
  std::vector<AtomicString> class_names_;
  Member<ElementRareData> rare_data_;
};

class Document final : public ContainerNode {
 public:
  Document(Isolate* isolate, Window* window);
  void Trace(Visitor* visitor) const override;

  Window* window() const { return window_.Get(); }
  // The window this document is displayed in, or null for detached
  // documents such as template contents owners.
  Window* default_view() const {
    return has_browsing_context_ ? window() : nullptr;
  }
  void set_has_browsing_context(bool value) { has_browsing_context_ = value; }

  Element* document_element() const;
  Element* body() const;
  Element* head() const;

  Element* CreateElement(Isolate* isolate, Namespace ns,
                         const AtomicString& local_name);
  Node* CreateTextNode(Isolate* isolate, const String& data);
  Node* CreateComment(Isolate* isolate, const String& data);
  DocumentFragment* CreateDocumentFragment(Isolate* isolate);

  Element* GetElementById(const AtomicString& id) const;
  void AddElementId(const AtomicString& id, Element* element);
  void RemoveElementId(const AtomicString& id, Element* element);

  Element* active_element() const;
  void set_focused_element(Element* element);
  Element* focused_element() const { return focused_element_.Get(); }

  // Bumped on every tree mutation; live collections use it to invalidate
  // their caches.
  uint64_t tree_version() const { return tree_version_; }
  void IncrementTreeVersion() { ++tree_version_; }
  // Bumped on attribute, inline style and text changes. Together with the
  // tree version it keys the geometry cache below.
  void InvalidateLayout() { ++style_version_; }
  uint64_t layout_key() const { return tree_version_ + style_version_; }
  // Geometry computed by layout.cc for the current layout_key(): x, y,
  // width, height per element.
  // Caches owned by css.cc and layout.cc, valid for one layout_key().
  std::shared_ptr<void> style_data;
  std::shared_ptr<void> layout_data;
  Member<AdoptedStyleSheets> adopted_style_sheets;

  AtomicString ready_state = "loading";
  String title;
  String cookie;
  Member<Element> current_script;
  // The document's Selection object, created on first use.
  Member<ScriptWrappable> selection;
  // The inert document that owns template contents.
  Document* TemplateContentsOwner(Isolate* isolate);

 private:
  Member<Window> window_;
  bool has_browsing_context_ = false;
  Member<Element> focused_element_;
  Member<Document> template_contents_owner_;
  uint64_t tree_version_ = 0;
  uint64_t style_version_ = 0;
  // Elements are connected while they are in the map, which keeps them alive.
  // Weak: connected elements are kept alive by the tree and leave the map
  // when they are disconnected.
  absl::flat_hash_map<AtomicString, std::vector<cppgc::WeakPersistent<Element>>,
                      AtomicString::Hash>
      ids_;
};

// --- Collections -------------------------------------------------------------

// An array-like collection of nodes. Static lists (querySelectorAll) hold
// their nodes; live lists (childNodes, children, getElementsBy*) recompute
// on demand and cache per document tree version.
class NodeList final : public ScriptWrappable {
 public:
  enum Kind : uint8_t {
    kStatic,
    kChildNodes,
    kChildren,
    kByTagName,
    kByClassName,
  };

  NodeList(Interface interface, Kind kind, Node* root, AtomicString name)
      : ScriptWrappable(interface),
        kind_(kind),
        root_(root),
        name_(std::move(name)) {}
  void Trace(Visitor* visitor) const override;
  Window* WrapperWindow() const override;

  static NodeList* CreateStatic(Isolate* isolate,
                                const std::vector<Node*>& nodes);
  static NodeList* CreateStatic(Isolate* isolate,
                                const std::vector<Element*>& nodes);

  uint32_t length();
  Node* item(uint32_t index);
  // Searches [start_index, min(end_index, length)) for |needle|, writes the
  // collection length to *out_length, and returns the matching index or
  // UINT32_MAX if not found.
  uint32_t IndexOf(Node* needle, uint32_t start_index, uint32_t end_index,
                   uint32_t* out_length);
  // For kByClassName lists: the space-separated class names to match.
  void SetClassNames(std::string_view names);

 private:
  void Update();

  Kind kind_;
  Member<Node> root_;
  AtomicString name_;
  std::vector<AtomicString> class_names_;
  uint64_t cached_version_ = ~uint64_t{0};
  MOCK_DOM_TRACED_CONTAINER
  std::vector<Member<Node>> items_;
};

// --- Window ------------------------------------------------------------------

class Window final : public EventTarget {
 public:
  Window(Isolate* isolate, Local<Context> context);
  // Creates the document and the other per-window objects.
  void Initialize(Isolate* isolate);
  void Trace(Visitor* visitor) const override;

  // Context embedder data slots used to find the window of a realm.
  static constexpr int kWindowEmbedderDataIndex = 2;
  static constexpr int kWindowHolderEmbedderDataIndex = 3;
  static void Install(Isolate* isolate, Local<Context> context,
                      Local<Object> holder, Window* window);

  Window* AsWindow() override { return this; }
  Window* WrapperWindow() const override { return const_cast<Window*>(this); }

  static Window* From(Local<Context> context);
  static Window* Current(Isolate* isolate) {
    return From(isolate->GetCurrentContext());
  }

  Local<Context> context(Isolate* isolate) const {
    return context_.Get(isolate);
  }
  Document* document() const { return document_.Get(); }
  CustomElementRegistry* custom_elements() const {
    return custom_elements_.Get();
  }
  Location* location() const { return location_.Get(); }
  History* history() const { return history_.Get(); }
  Storage* local_storage() const { return local_storage_.Get(); }
  Storage* session_storage() const { return session_storage_.Get(); }

  // Reports an exception that escaped a callback (listener, timer, custom
  // element reaction...) by firing an `error` event at the window. If nothing
  // handles it, it is printed and counted as an uncaught error.
  void ReportException(Isolate* isolate, Local<Value> exception,
                       Local<Message> message);

  // Reports use of a browser feature the mock DOM does not implement, as an
  // uncaught error, so that it cannot go unnoticed.
  void ReportUnsupported(Isolate* isolate, const char* what);

  // Hook used by the page loader to learn about script, link, img and iframe
  // elements that get connected or have their URL changed. See d8.mockDom in
  // the bindings.
  TracedReference<Function> resource_callback;
  // window.frameElement, set by the loader for frames.
  TracedReference<Value> frame_element;
  // window.indexedDB, created on first use (see bindings-indexeddb.cc).
  Member<ScriptWrappable> indexed_db;
  int inner_width = 800;
  int inner_height = 600;
  // Set when the frame this window belongs to has been removed. Timers and
  // queued tasks of a detached window no longer run.
  bool detached = false;

 private:
  TracedReference<Context> context_;
  Member<Document> document_;
  Member<CustomElementRegistry> custom_elements_;
  Member<Location> location_;
  Member<History> history_;
  Member<Storage> local_storage_;
  Member<Storage> session_storage_;
};

// Number of exceptions that reached ReportException() without being handled
// by an `error` listener. The shell uses it for its exit code.
int UncaughtErrorCount();

// --- Custom elements ---------------------------------------------------------

class CustomElementDefinition final
    : public cppgc::GarbageCollected<CustomElementDefinition> {
 public:
  void Trace(Visitor* visitor) const;

  AtomicString name;
  TracedReference<Function> constructor;
  TracedReference<Function> connected_callback;
  TracedReference<Function> disconnected_callback;
  TracedReference<Function> adopted_callback;
  TracedReference<Function> attribute_changed_callback;
  std::vector<AtomicString> observed_attributes;
  // Elements being upgraded by a constructor call that is in progress.
  std::vector<cppgc::Persistent<Element>> construction_stack;
  Member<CustomElementDefinition> next;

  bool Observes(const AtomicString& attribute) const;
};

class CustomElementRegistry final : public ScriptWrappable {
 public:
  explicit CustomElementRegistry(Window* window)
      : ScriptWrappable(Interface::kCustomElementRegistry), window_(window) {}
  void Trace(Visitor* visitor) const override;
  Window* WrapperWindow() const override { return window_.Get(); }

  CustomElementDefinition* Lookup(const AtomicString& name) const;
  CustomElementDefinition* LookupByConstructor(Isolate* isolate,
                                               Local<Value> constructor) const;
  void Define(Isolate* isolate, const AtomicString& name,
              Local<Function> constructor, ExceptionState* exception_state);
  Local<Value> WhenDefined(Isolate* isolate, const AtomicString& name);
  // Upgrades every not-yet-upgraded custom element in |root|'s subtree.
  void UpgradeSubtree(Isolate* isolate, Node* root);

  static bool IsValidName(const AtomicString& name);

 private:
  struct PendingPromise;
  Member<Window> window_;
  Member<CustomElementDefinition> definitions_;
  MOCK_DOM_TRACED_CONTAINER
  absl::flat_hash_map<AtomicString, TracedReference<Promise::Resolver>,
                      AtomicString::Hash>
      when_defined_;
  bool define_running_ = false;
};

// Defers custom element reactions until the outermost DOM operation has left
// the tree in a consistent state, then runs them. Mirrors [CEReactions].
class CEReactionsScope {
 public:
  explicit CEReactionsScope(Isolate* isolate);
  ~CEReactionsScope();

  enum Kind {
    kUpgrade,
    kConnected,
    kDisconnected,
    kAttributeChanged,
    // Not a custom element reaction: tells the page loader that a script,
    // link, img or iframe was connected or had its URL changed. It rides on
    // the same queue because it also has to wait for a consistent tree.
    kResourceChanged,
  };
  static void Enqueue(Isolate* isolate, Element* element, Kind kind,
                      AtomicString name = AtomicString(),
                      AtomicString old_value = AtomicString(),
                      AtomicString new_value = AtomicString());

 private:
  Isolate* isolate_;
};

// Upgrades |element| with |definition| by running its constructor.
void UpgradeElement(Isolate* isolate, Element* element,
                    CustomElementDefinition* definition);

// Style invalidation, implemented in css.cc. Computed styles are kept
// between changes; these drop the ones a change may have affected.
// InvalidateStyle covers |root| and everything below it, including shadow
// trees.
void InvalidateStyle(Node* root);
void InvalidateStyleIfCached(Node* root);
void DropCachedStyle(Node* node);
void InvalidateAllStyles(Document* document);

// Adopts |root| and its shadow-including descendants into |document|,
// dropping any cached styles they still had in their previous document.
void AdoptSubtree(Node* root, Document* document);

// Splits |text| on HTML whitespace and appends the tokens to |out|.
void SplitOnHtmlSpace(std::string_view text, std::vector<AtomicString>* out);

// --- Wrapping ----------------------------------------------------------------

// Returns the wrapper for |object|, creating it on first use. Null maps to JS
// null.
Local<Value> ToV8(Isolate* isolate, ScriptWrappable* object);
// Returns the wrapped object if |value| is a wrapper for (a subtype of)
// |interface|, else null.
ScriptWrappable* ToWrappable(Isolate* isolate, Local<Value> value,
                             Interface interface);
template <typename T>
T* To(Isolate* isolate, Local<Value> value, Interface interface) {
  return static_cast<T*>(ToWrappable(isolate, value, interface));
}

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_DOM_H_
