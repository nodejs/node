// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/dom.h"

#include <algorithm>
#include <cstdio>
#include <limits>

#include "experimental/d8/mock-dom/css.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/objects.h"
#include "include/v8-container.h"
#include "include/v8-exception.h"
#include "include/v8-message.h"
#include "include/v8-primitive.h"
#include "include/v8-promise.h"
#include "src/base/logging.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace mock_dom {

// --- Interfaces --------------------------------------------------------------

namespace {

const char* const kInterfaceNames[] = {
#define V(Name, Parent) #Name,
    MOCK_DOM_INTERFACE_LIST(V)
#undef V
};

const Interface kInterfaceParents[] = {
#define V(Name, Parent) Interface::k##Parent,
    MOCK_DOM_INTERFACE_LIST(V)
#undef V
};

IsolateData* g_isolate_data = nullptr;

}  // namespace

const char* InterfaceName(Interface interface) {
  return kInterfaceNames[static_cast<int>(interface)];
}

Interface InterfaceParent(Interface interface) {
  return kInterfaceParents[static_cast<int>(interface)];
}

bool InterfaceInheritsFrom(Interface interface, Interface ancestor) {
  while (interface != Interface::kNone) {
    if (interface == ancestor) return true;
    interface = InterfaceParent(interface);
  }
  return false;
}

// --- Isolate data ------------------------------------------------------------

IsolateData* IsolateData::Get() { return g_isolate_data; }

void IsolateData::Create(Isolate* isolate) {
  CHECK_NULL(g_isolate_data);
  g_isolate_data = new IsolateData();
  g_isolate_data->isolate = isolate;
}

void IsolateData::Destroy(Isolate* isolate) {
  if (!g_isolate_data || g_isolate_data->isolate != isolate) return;
  delete g_isolate_data;
  g_isolate_data = nullptr;
  DisposeStringCache(isolate);
}

int UncaughtErrorCount() {
  return g_isolate_data ? g_isolate_data->uncaught_error_count : 0;
}

double MonotonicNowMs() {
  static const base::TimeTicks start = base::TimeTicks::Now();
  return (base::TimeTicks::Now() - start).InMillisecondsF();
}

MaybeLocal<Value> CallAndReport(Isolate* isolate, Window* window,
                                Local<Function> function, Local<Value> receiver,
                                int argc, Local<Value> argv[]) {
  Local<Context> context;
  if (!function->GetCreationContext(isolate).ToLocal(&context)) {
    context = isolate->GetCurrentContext();
  }
  TryCatch try_catch(isolate);
  try_catch.SetVerbose(false);
  Local<Value> result;
  if (function->Call(context, receiver, argc, argv).ToLocal(&result)) {
    return result;
  }
  if (try_catch.HasTerminated()) {
    try_catch.ReThrow();
    return {};
  }
  if (!window) window = Window::From(context);
  if (window) {
    window->ReportException(isolate, try_catch.Exception(),
                            try_catch.Message());
  }
  return {};
}

// --- Event listeners ---------------------------------------------------------

void EventTarget::AddEventListener(Isolate* isolate, const AtomicString& type,
                                   Local<Value> callback, bool capture,
                                   bool once, bool passive) {
  EventListener* tail = nullptr;
  for (EventListener* l = listeners_; l; l = l->next) {
    if (!l->is_attribute && l->capture == capture && l->type == type &&
        l->callback.Get(isolate)->StrictEquals(callback)) {
      return;
    }
    tail = l;
  }
  EventListener* listener = MakeGarbageCollected<EventListener>(
      isolate, isolate, type, callback, capture);
  listener->once = once;
  listener->passive = passive;
  if (tail) {
    tail->next = listener;
  } else {
    listeners_ = listener;
  }
}

void EventTarget::RemoveEventListener(Isolate* isolate,
                                      const AtomicString& type,
                                      Local<Value> callback, bool capture) {
  EventListener* prev = nullptr;
  for (EventListener* l = listeners_; l; prev = l, l = l->next) {
    if (l->is_attribute || l->capture != capture || l->type != type ||
        !l->callback.Get(isolate)->StrictEquals(callback)) {
      continue;
    }
    l->removed = true;
    if (prev) {
      prev->next = l->next;
    } else {
      listeners_ = l->next;
    }
    return;
  }
}

Local<Value> EventTarget::GetAttributeListener(Isolate* isolate,
                                               const AtomicString& type) {
  for (EventListener* l = listeners_; l; l = l->next) {
    if (l->is_attribute && l->type == type) return l->callback.Get(isolate);
  }
  return Null(isolate);
}

void EventTarget::SetAttributeListener(Isolate* isolate,
                                       const AtomicString& type,
                                       Local<Value> callback) {
  bool is_callable = callback->IsObject();
  EventListener* prev = nullptr;
  EventListener* tail = nullptr;
  for (EventListener* l = listeners_; l; prev = l, l = l->next) {
    tail = l;
    if (!l->is_attribute || l->type != type) continue;
    if (is_callable) {
      // Replacing the handler keeps its position in the listener list.
      l->callback.Reset(isolate, callback);
    } else {
      l->removed = true;
      if (prev) {
        prev->next = l->next;
      } else {
        listeners_ = l->next;
      }
    }
    return;
  }
  if (!is_callable) return;
  EventListener* listener = MakeGarbageCollected<EventListener>(
      isolate, isolate, type, callback, false);
  listener->is_attribute = true;
  if (tail) {
    tail->next = listener;
  } else {
    listeners_ = listener;
  }
}

bool EventTarget::HasListeners(const AtomicString& type) const {
  for (EventListener* l = listeners_; l; l = l->next) {
    if (l->type == type) return true;
  }
  return false;
}

void EventTarget::FireListeners(Isolate* isolate, Event* event,
                                bool capture_phase, bool at_target) {
  if (!listeners_) return;
  // Listeners added while dispatching must not run for this event, and the
  // list can be mutated by the listeners, so walk a snapshot.
  std::vector<cppgc::Persistent<EventListener>> snapshot;
  for (EventListener* l = listeners_; l; l = l->next) {
    if (l->type == event->type() && l->capture == capture_phase) {
      snapshot.emplace_back(l);
    }
  }
  if (snapshot.empty()) return;

  HandleScope handle_scope(isolate);
  // Events fired from a task arrive with no realm entered. Wrappers for the
  // event are then created in the realm of the first listener.
  Local<Context> entered_context;
  if (!isolate->InContext()) {
    Local<Value> first = snapshot[0]->callback.Get(isolate);
    if (first->IsObject()) {
      USE(first.As<Object>()->GetCreationContext(isolate).ToLocal(
          &entered_context));
    }
    if (entered_context.IsEmpty()) return;
    entered_context->Enter();
  }
  struct ExitContext {
    Local<Context> context;
    ~ExitContext() {
      if (!context.IsEmpty()) context->Exit();
    }
  } exit_context{entered_context};
  Local<Value> event_wrapper = ToV8(isolate, event);
  Local<Value> receiver = ToV8(isolate, this);
  for (const auto& persistent : snapshot) {
    EventListener* listener = persistent.Get();
    if (listener->removed) continue;
    if (listener->once) {
      RemoveEventListener(isolate, listener->type,
                          listener->callback.Get(isolate), listener->capture);
    }
    Local<Value> callback = listener->callback.Get(isolate);
    Local<Function> function;
    Local<Value> this_value = receiver;
    if (callback->IsFunction()) {
      function = callback.As<Function>();
    } else if (callback->IsObject()) {
      // An EventListener object: look up handleEvent at call time.
      Local<Context> context;
      if (!callback.As<Object>()->GetCreationContext(isolate).ToLocal(
              &context)) {
        continue;
      }
      TryCatch try_catch(isolate);
      Local<Value> handle_event;
      if (!callback.As<Object>()
               ->Get(context,
                     v8::String::NewFromUtf8Literal(isolate, "handleEvent"))
               .ToLocal(&handle_event)) {
        if (Window* window = Window::From(context)) {
          window->ReportException(isolate, try_catch.Exception(),
                                  try_catch.Message());
        }
        continue;
      }
      if (!handle_event->IsFunction()) continue;
      function = handle_event.As<Function>();
      this_value = callback;
    } else {
      continue;
    }

    event->in_passive_listener = listener->passive;
    Local<Value> result;
    bool is_window_onerror = listener->is_attribute && AsWindow() &&
                             event->interface() == Interface::kErrorEvent;
    if (is_window_onerror) {
      // window.onerror has its own five-argument signature, and returning
      // true (not false) marks the error as handled.
      Local<Value> args[] = {
          ToV8String(isolate, event->string("message")),
          ToV8String(isolate, event->string("filename")),
          Number::New(isolate, event->number("lineno")),
          Number::New(isolate, event->number("colno")),
          event->any_slots[event->FieldIndex("error")].Get(isolate)};
      if (args[4].IsEmpty()) args[4] = Undefined(isolate);
      if (CallAndReport(isolate, nullptr, function, this_value, 5, args)
              .ToLocal(&result) &&
          result->IsTrue()) {
        event->default_prevented = true;
      }
    } else {
      Local<Value> args[] = {event_wrapper};
      if (CallAndReport(isolate, nullptr, function, this_value, 1, args)
              .ToLocal(&result) &&
          listener->is_attribute && result->IsFalse() && event->cancelable) {
        event->default_prevented = true;
      }
    }
    event->in_passive_listener = false;
    if (isolate->IsExecutionTerminating()) return;
    if (event->immediate_propagation_stopped) break;
  }
}

bool EventTarget::DispatchEvent(Isolate* isolate, Event* event) {
  if (Node* node = AsNode()) return node->DispatchNodeEvent(isolate, event);
  event->dispatching = true;
  event->target = this;
  event->current_target = this;
  event->phase = Event::kAtTarget;
  FireListeners(isolate, event, true, true);
  if (!event->propagation_stopped) FireListeners(isolate, event, false, true);
  event->phase = Event::kNone;
  event->current_target = nullptr;
  event->dispatching = false;
  event->propagation_stopped = false;
  event->immediate_propagation_stopped = false;
  return !event->default_prevented;
}

// --- Event -------------------------------------------------------------------

namespace {

using F = EventField;

struct EventFieldTable {
  Interface interface;
  std::vector<EventField> own_fields;
};

const std::vector<EventFieldTable>& OwnEventFields() {
  static const std::vector<EventFieldTable>* tables =
      new std::vector<EventFieldTable>{
          {Interface::kIDBVersionChangeEvent,
           {{"oldVersion", F::kNumber}, {"newVersion", F::kAny}}},
          {Interface::kUIEvent,
           {{"detail", F::kNumber}, {"view", F::kAny}, {"which", F::kNumber}}},
          {Interface::kMouseEvent,
           {{"screenX", F::kNumber},
            {"screenY", F::kNumber},
            {"clientX", F::kNumber},
            {"clientY", F::kNumber},
            {"movementX", F::kNumber},
            {"movementY", F::kNumber},
            {"button", F::kNumber},
            {"buttons", F::kNumber},
            {"ctrlKey", F::kBool},
            {"shiftKey", F::kBool},
            {"altKey", F::kBool},
            {"metaKey", F::kBool},
            {"relatedTarget", F::kTarget}}},
          {Interface::kPointerEvent,
           {{"pointerId", F::kNumber},
            {"width", F::kNumber},
            {"height", F::kNumber},
            {"pressure", F::kNumber},
            {"pointerType", F::kString},
            {"isPrimary", F::kBool}}},
          {Interface::kWheelEvent,
           {{"deltaX", F::kNumber},
            {"deltaY", F::kNumber},
            {"deltaZ", F::kNumber},
            {"deltaMode", F::kNumber}}},
          {Interface::kDragEvent, {{"dataTransfer", F::kAny}}},
          {Interface::kKeyboardEvent,
           {{"key", F::kString},
            {"code", F::kString},
            {"location", F::kNumber},
            {"keyCode", F::kNumber},
            {"charCode", F::kNumber},
            {"ctrlKey", F::kBool},
            {"shiftKey", F::kBool},
            {"altKey", F::kBool},
            {"metaKey", F::kBool},
            {"repeat", F::kBool},
            {"isComposing", F::kBool}}},
          {Interface::kInputEvent,
           {{"data", F::kString},
            {"inputType", F::kString},
            {"isComposing", F::kBool}}},
          {Interface::kFocusEvent, {{"relatedTarget", F::kTarget}}},
          {Interface::kCompositionEvent, {{"data", F::kString}}},
          {Interface::kCustomEvent, {{"detail", F::kAny}}},
          {Interface::kErrorEvent,
           {{"message", F::kString},
            {"filename", F::kString},
            {"lineno", F::kNumber},
            {"colno", F::kNumber},
            {"error", F::kAny}}},
          {Interface::kHashChangeEvent,
           {{"oldURL", F::kString}, {"newURL", F::kString}}},
          {Interface::kPopStateEvent, {{"state", F::kAny}}},
          {Interface::kMessageEvent,
           {{"data", F::kAny},
            {"origin", F::kString},
            {"lastEventId", F::kString},
            {"source", F::kAny},
            {"ports", F::kAny}}},
          {Interface::kPromiseRejectionEvent,
           {{"promise", F::kAny}, {"reason", F::kAny}}},
          {Interface::kSubmitEvent, {{"submitter", F::kTarget}}},
          {Interface::kProgressEvent,
           {{"lengthComputable", F::kBool},
            {"loaded", F::kNumber},
            {"total", F::kNumber}}},
          {Interface::kTransitionEvent,
           {{"propertyName", F::kString},
            {"elapsedTime", F::kNumber},
            {"pseudoElement", F::kString}}},
          {Interface::kAnimationEvent,
           {{"animationName", F::kString},
            {"elapsedTime", F::kNumber},
            {"pseudoElement", F::kString}}},
          {Interface::kClipboardEvent, {{"clipboardData", F::kAny}}},
      };
  return *tables;
}

}  // namespace

const std::vector<EventField>& EventFieldsFor(Interface interface) {
  static std::vector<EventField>* cache[kInterfaceCount] = {};
  int index = static_cast<int>(interface);
  if (cache[index]) return *cache[index];
  auto* fields = new std::vector<EventField>();
  Interface parent = InterfaceParent(interface);
  if (parent != Interface::kNone) *fields = EventFieldsFor(parent);
  for (const EventFieldTable& table : OwnEventFields()) {
    if (table.interface != interface) continue;
    for (const EventField& field : table.own_fields) {
      // A derived interface redeclaring a member reuses the inherited slot.
      bool inherited = false;
      for (const EventField& existing : *fields) {
        if (strcmp(existing.name, field.name) == 0) inherited = true;
      }
      if (!inherited) fields->push_back(field);
    }
  }
  cache[index] = fields;
  return *fields;
}

Event::Event(Isolate* isolate, Interface interface, AtomicString type)
    : ScriptWrappable(interface),
      time_stamp(MonotonicNowMs()),
      type_(std::move(type)) {
  const std::vector<EventField>& fields = EventFieldsFor(interface);
  slots.resize(fields.size());
  any_slots.resize(fields.size());
  target_slots.resize(fields.size());
  for (size_t i = 0; i < fields.size(); ++i) {
    // Nullable string members default to null, the others to "".
    if (fields[i].kind == F::kString && strcmp(fields[i].name, "data") != 0) {
      slots[i].string = String::Empty();
    }
  }
}

void Event::Trace(Visitor* visitor) const {
  if (DeferTraceIfConcurrent(visitor, this)) return;
  ScriptWrappable::Trace(visitor);
  visitor->Trace(target);
  visitor->Trace(current_target);
  visitor->Trace(path);
  for (const auto& slot : any_slots) visitor->Trace(slot);
  for (const auto& slot : target_slots) visitor->Trace(slot);
}

int Event::FieldIndex(const char* name) const {
  const std::vector<EventField>& fields = EventFieldsFor(interface());
  for (size_t i = 0; i < fields.size(); ++i) {
    if (strcmp(fields[i].name, name) == 0) return static_cast<int>(i);
  }
  return -1;
}

double Event::number(const char* name) const {
  int index = FieldIndex(name);
  return index < 0 ? 0 : slots[index].number;
}

void Event::set_number(const char* name, double value) {
  int index = FieldIndex(name);
  CHECK_GE(index, 0);
  slots[index].number = value;
}

String Event::string(const char* name) const {
  int index = FieldIndex(name);
  return index < 0 ? String() : slots[index].string;
}

void Event::set_string(const char* name, String value) {
  int index = FieldIndex(name);
  CHECK_GE(index, 0);
  slots[index].string = std::move(value);
}

// --- Node --------------------------------------------------------------------

Node::Node(Document* document, Interface interface, Type type, uint16_t flags)
    : EventTarget(interface), type_(type), flags_(flags), document_(document) {}

void Node::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  visitor->Trace(document_);
  visitor->Trace(parent_);
  visitor->Trace(prev_);
  visitor->Trace(next_);
  visitor->Trace(observers_);
}

Window* Node::WrapperWindow() const {
  if (document_) return document_->window();
  // A Document is its own owner.
  return static_cast<const Document*>(this)->window();
}

Element* Node::parent_element() const {
  ContainerNode* p = parent();
  return p && p->IsElement() ? static_cast<Element*>(p) : nullptr;
}

Node* Node::first_child() const {
  return IsContainer() ? static_cast<const ContainerNode*>(this)->first()
                       : nullptr;
}

Node* Node::last_child() const {
  return IsContainer() ? static_cast<const ContainerNode*>(this)->last()
                       : nullptr;
}

Element* Node::previous_element_sibling() const {
  for (Node* n = previous_sibling(); n; n = n->previous_sibling()) {
    if (n->IsElement()) return static_cast<Element*>(n);
  }
  return nullptr;
}

Element* Node::next_element_sibling() const {
  for (Node* n = next_sibling(); n; n = n->next_sibling()) {
    if (n->IsElement()) return static_cast<Element*>(n);
  }
  return nullptr;
}

Node* Node::Root() {
  Node* node = this;
  while (node->parent()) node = node->parent();
  return node;
}

ShadowRoot* Node::ContainingShadowRoot() {
  Node* root = Root();
  return root->IsShadowRoot() ? static_cast<ShadowRoot*>(root) : nullptr;
}

bool Node::Contains(const Node* other) const {
  for (const Node* n = other; n; n = n->parent()) {
    if (n == this) return true;
  }
  return false;
}

bool Node::ShadowIncludingContains(const Node* other) const {
  const Node* n = other;
  while (n) {
    if (n == this) return true;
    if (n->parent()) {
      n = n->parent();
    } else if (n->IsShadowRoot()) {
      n = static_cast<const ShadowRoot*>(n)->host();
    } else {
      return false;
    }
  }
  return false;
}

Node* Node::NextInPreOrder(const Node* stay_within) const {
  if (Node* child = first_child()) return child;
  return NextSkippingChildren(stay_within);
}

Node* Node::NextSkippingChildren(const Node* stay_within) const {
  const Node* node = this;
  while (node && node != stay_within) {
    if (Node* next = node->next_sibling()) return next;
    node = node->parent();
  }
  return nullptr;
}

String Node::NodeName() const {
  switch (type_) {
    case kElementNode:
      return static_cast<const Element*>(this)->TagName();
    case kTextNode:
      return AtomicString("#text");
    case kCommentNode:
      return AtomicString("#comment");
    case kDocumentNode:
      return AtomicString("#document");
    case kDocumentTypeNode:
      return static_cast<const DocumentType*>(this)->name;
    case kDocumentFragmentNode:
      return AtomicString("#document-fragment");
  }
  UNREACHABLE();
}

void Node::AppendTextContent(std::string* utf8) const {
  for (const Node* n = first_child(); n; n = n->NextInPreOrder(this)) {
    if (n->IsText()) {
      utf8->append(static_cast<const CharacterData*>(n)->data().ToUtf8());
    }
  }
}

String Node::TextContent() const {
  if (IsCharacterData()) return static_cast<const CharacterData*>(this)->data();
  if (IsDocument() || type_ == kDocumentTypeNode) return String();
  // The common single-text-child case returns the child's string unchanged,
  // so that JS sees the same cached V8 string.
  Node* child = first_child();
  if (!child) return String::Empty();
  if (child->IsText() && !child->next_sibling()) {
    return static_cast<CharacterData*>(child)->data();
  }
  std::string utf8;
  AppendTextContent(&utf8);
  return String::FromUtf8(utf8);
}

void Node::SetTextContent(Isolate* isolate, const String& text) {
  if (IsCharacterData()) {
    static_cast<CharacterData*>(this)->SetData(isolate, text);
    return;
  }
  if (IsDocument() || type_ == kDocumentTypeNode) return;
  CEReactionsScope reactions(isolate);
  ContainerNode* container = static_cast<ContainerNode*>(this);
  container->RemoveChildren(isolate);
  if (!text.empty()) {
    ExceptionState exception_state;
    container->AppendChild(isolate, document()->CreateTextNode(isolate, text),
                           &exception_state);
  }
}

String Node::NodeValue() const {
  return IsCharacterData() ? static_cast<const CharacterData*>(this)->data()
                           : String();
}

void Node::Remove(Isolate* isolate) {
  if (ContainerNode* p = parent()) {
    ExceptionState exception_state;
    p->RemoveChild(isolate, this, &exception_state);
  }
}

uint16_t Node::CompareDocumentPosition(const Node* other) const {
  constexpr uint16_t kDisconnected = 1, kPreceding = 2, kFollowing = 4,
                     kContains = 8, kContainedBy = 16,
                     kImplementationSpecific = 32;
  if (this == other) return 0;
  MOCK_DOM_LOCAL_NODES
  std::vector<const Node*> chain1;
  MOCK_DOM_LOCAL_NODES
  std::vector<const Node*> chain2;
  for (const Node* n = this; n; n = n->parent()) chain1.push_back(n);
  for (const Node* n = other; n; n = n->parent()) chain2.push_back(n);
  if (chain1.back() != chain2.back()) {
    return kDisconnected | kImplementationSpecific |
           (this < other ? kFollowing : kPreceding);
  }
  size_t i1 = chain1.size(), i2 = chain2.size();
  while (i1 > 0 && i2 > 0 && chain1[i1 - 1] == chain2[i2 - 1]) {
    --i1;
    --i2;
  }
  if (i1 == 0) return kContainedBy | kFollowing;  // this contains other
  if (i2 == 0) return kContains | kPreceding;     // other contains this
  const Node* child1 = chain1[i1 - 1];
  const Node* child2 = chain2[i2 - 1];
  for (const Node* n = child1->next_sibling(); n; n = n->next_sibling()) {
    if (n == child2) return kFollowing;
  }
  return kPreceding;
}

bool Node::IsEqualNode(const Node* other) const {
  if (!other || type_ != other->type_) return false;
  if (IsCharacterData()) {
    if (static_cast<const CharacterData*>(this)->data() !=
        static_cast<const CharacterData*>(other)->data()) {
      return false;
    }
  } else if (IsElement()) {
    const Element* a = static_cast<const Element*>(this);
    const Element* b = static_cast<const Element*>(other);
    if (a->ns() != b->ns() || a->local_name() != b->local_name() ||
        a->attributes().size() != b->attributes().size()) {
      return false;
    }
    for (const Attribute& attribute : a->attributes()) {
      if (b->GetAttribute(attribute.name) != attribute.value ||
          !b->HasAttribute(attribute.name)) {
        return false;
      }
    }
  }
  const Node* c1 = first_child();
  const Node* c2 = other->first_child();
  while (c1 && c2) {
    if (!c1->IsEqualNode(c2)) return false;
    c1 = c1->next_sibling();
    c2 = c2->next_sibling();
  }
  return !c1 && !c2;
}

void Node::Normalize(Isolate* isolate) {
  Node* node = first_child();
  while (node) {
    Node* next = node->NextInPreOrder(this);
    if (node->IsText()) {
      CharacterData* text = static_cast<CharacterData*>(node);
      if (text->data().empty()) {
        next = node->NextSkippingChildren(this);
        node->Remove(isolate);
      } else if (Node* sibling = node->next_sibling();
                 sibling && sibling->IsText()) {
        std::string merged = text->data().ToUtf8();
        while (sibling && sibling->IsText()) {
          merged += static_cast<CharacterData*>(sibling)->data().ToUtf8();
          Node* following = sibling->next_sibling();
          sibling->Remove(isolate);
          sibling = following;
        }
        text->SetData(isolate, String::FromUtf8(merged));
        next = node->NextSkippingChildren(this);
      }
    }
    node = next;
  }
}

MutationObserverRegistration* Node::observer_registrations() const {
  return observers_.Get();
}

void Node::set_observer_registrations(MutationObserverRegistration* head) {
  observers_ = head;
}

Node* Node::Clone(Isolate* isolate, Document* document, bool deep) const {
  Node* clone = nullptr;
  switch (type_) {
    case kElementNode: {
      const Element* source = static_cast<const Element*>(this);
      Element* element =
          document->CreateElement(isolate, source->ns(), source->local_name());
      source->SynchronizeAttributes();
      for (const Attribute& attribute : source->attributes()) {
        element->ParserSetAttribute(attribute.name, attribute.value);
      }
      if (ElementRareData* rare = source->rare_data()) {
        if (rare->has_dirty_value || rare->has_dirty_checked ||
            rare->has_dirty_selected) {
          ElementRareData* target = element->EnsureRareData(isolate);
          target->value = rare->value;
          target->has_dirty_value = rare->has_dirty_value;
          target->checked = rare->checked;
          target->has_dirty_checked = rare->has_dirty_checked;
          target->selected = rare->selected;
          target->has_dirty_selected = rare->has_dirty_selected;
        }
        if (rare->template_content && deep) {
          DocumentFragment* content = element->TemplateContent(isolate);
          for (Node* child = rare->template_content->first(); child;
               child = child->next_sibling()) {
            content->ParserAppendChild(
                isolate, child->Clone(isolate, content->document(), true));
          }
        }
      }
      clone = element;
      break;
    }
    case kTextNode:
      clone = document->CreateTextNode(
          isolate, static_cast<const CharacterData*>(this)->data());
      break;
    case kCommentNode:
      clone = document->CreateComment(
          isolate, static_cast<const CharacterData*>(this)->data());
      break;
    case kDocumentFragmentNode:
      clone = document->CreateDocumentFragment(isolate);
      break;
    case kDocumentTypeNode:
      clone = MakeGarbageCollected<DocumentType>(
          isolate, document, static_cast<const DocumentType*>(this)->name);
      break;
    case kDocumentNode:
      return nullptr;
  }
  if (deep) {
    ContainerNode* container =
        clone->IsContainer() ? static_cast<ContainerNode*>(clone) : nullptr;
    for (Node* child = first_child(); child; child = child->next_sibling()) {
      container->ParserAppendChild(isolate,
                                   child->Clone(isolate, document, true));
    }
  }
  return clone;
}

void CharacterData::SetData(Isolate* isolate, const String& data) {
  String old = data_;
  data_ = data.is_null() ? String::Empty() : data;
  document()->InvalidateLayout();
  if (Element* owner = parent_element(); owner && owner->HasTag("style")) {
    InvalidateAllStyles(document());
  }
  if (IsolateData::Get()->observer_registration_count) {
    QueueCharacterDataMutation(isolate, this, old);
  }
}

// --- ContainerNode -----------------------------------------------------------

void ContainerNode::Trace(Visitor* visitor) const {
  Node::Trace(visitor);
  visitor->Trace(first_);
  visitor->Trace(last_);
  visitor->Trace(child_nodes_list);
  visitor->Trace(children_list);
}

Element* ContainerNode::first_element_child() const {
  for (Node* n = first(); n; n = n->next_sibling()) {
    if (n->IsElement()) return static_cast<Element*>(n);
  }
  return nullptr;
}

Element* ContainerNode::last_element_child() const {
  for (Node* n = last(); n; n = n->previous_sibling()) {
    if (n->IsElement()) return static_cast<Element*>(n);
  }
  return nullptr;
}

uint32_t ContainerNode::CountChildren() const {
  uint32_t count = 0;
  for (Node* n = first(); n; n = n->next_sibling()) ++count;
  return count;
}

uint32_t ContainerNode::CountElementChildren() const {
  uint32_t count = 0;
  for (Node* n = first(); n; n = n->next_sibling()) {
    if (n->IsElement()) ++count;
  }
  return count;
}

bool ContainerNode::EnsurePreInsertionValidity(
    Node* node, Node* child, ExceptionState* exception_state) const {
  if (node->ShadowIncludingContains(this)) {
    return exception_state->Throw("HierarchyRequestError",
                                  "The new child element contains the parent.");
  }
  if (child && child->parent() != this) {
    return exception_state->Throw(
        "NotFoundError",
        "The node before which the new node is to be inserted is not a child "
        "of this node.");
  }
  if (node->IsDocument()) {
    return exception_state->Throw(
        "HierarchyRequestError",
        "Nodes of type '#document' may not be inserted inside nodes of type '" +
            NodeName().ToUtf8() + "'.");
  }
  if (IsDocument()) {
    if (node->IsText()) {
      return exception_state->Throw(
          "HierarchyRequestError",
          "Nodes of type '#text' may not be inserted inside nodes of type "
          "'#document'.");
    }
    if (node->IsElement() && first_element_child() &&
        first_element_child() != node) {
      return exception_state->Throw("HierarchyRequestError",
                                    "Only one element on document allowed.");
    }
  }
  return true;
}

namespace {

// Walks |root| and its shadow-including descendants in tree order.
template <typename Callback>
void ForEachShadowIncludingDescendant(Node* root, bool in_shadow_tree,
                                      const Callback& callback) {
  for (Node* node = root; node; node = node->NextInPreOrder(root)) {
    callback(node, in_shadow_tree);
    if (node->IsElement()) {
      if (ShadowRoot* shadow = static_cast<Element*>(node)->shadow_root()) {
        ForEachShadowIncludingDescendant(shadow, true, callback);
      }
    }
  }
}

bool IsResourceElement(Element* element) {
  return element->IsHTML() &&
         (element->HasTag("script") || element->HasTag("link") ||
          element->HasTag("iframe") || element->HasTag("img"));
}

void NotifyInserted(Isolate* isolate, Node* root) {
  Document* document = root->document();
  bool in_shadow_tree = root->ContainingShadowRoot() != nullptr;
  ForEachShadowIncludingDescendant(
      root, in_shadow_tree, [&](Node* node, bool in_shadow) {
        node->set_flag(Node::kIsConnected, true);
        if (!node->IsElement()) return;
        Element* element = static_cast<Element*>(node);
        if (element->HasTag("style") || element->HasTag("link")) {
          InvalidateAllStyles(document);
        }
        if (!in_shadow && !element->id().is_null()) {
          document->AddElementId(element->id(), element);
        }
        if (element->has_flag(Node::kIsCustom)) {
          CEReactionsScope::Enqueue(isolate, element,
                                    CEReactionsScope::kConnected);
        } else if (element->has_flag(Node::kIsUndefinedCustom)) {
          CEReactionsScope::Enqueue(isolate, element,
                                    CEReactionsScope::kUpgrade);
        }
        if (IsResourceElement(element)) {
          CEReactionsScope::Enqueue(isolate, element,
                                    CEReactionsScope::kResourceChanged);
        }
      });
}

void NotifyRemoved(Isolate* isolate, Node* root, bool was_in_shadow_tree) {
  Document* document = root->document();
  ForEachShadowIncludingDescendant(
      root, was_in_shadow_tree, [&](Node* node, bool in_shadow) {
        node->set_flag(Node::kIsConnected, false);
        if (!node->IsElement()) return;
        Element* element = static_cast<Element*>(node);
        if (element->HasTag("style") || element->HasTag("link")) {
          InvalidateAllStyles(document);
        }
        if (!in_shadow && !element->id().is_null()) {
          document->RemoveElementId(element->id(), element);
        }
        if (document->focused_element() == element) {
          document->set_focused_element(nullptr);
        }
        if (element->has_flag(Node::kIsCustom)) {
          CEReactionsScope::Enqueue(isolate, element,
                                    CEReactionsScope::kDisconnected);
        }
      });
}

}  // namespace

void AdoptSubtree(Node* root, Document* document) {
  ForEachShadowIncludingDescendant(root, false, [&](Node* node, bool) {
    if (node->IsDocument()) return;
    if (node->document() != document) DropCachedStyle(node);
    node->AdoptInto(document);
  });
}

void ContainerNode::InsertInternal(Isolate* isolate, Node* node, Node* child) {
  DCHECK_NULL(node->parent());
  Document* document =
      IsDocument() ? static_cast<Document*>(this) : this->document();
  if (node->document() != document) AdoptSubtree(node, document);

  Node* prev = child ? child->prev_.Get() : last_.Get();
  node->parent_ = this;
  node->prev_ = prev;
  node->next_ = child;
  if (prev) {
    prev->next_ = node;
  } else {
    first_ = node;
  }
  if (child) {
    child->prev_ = node;
  } else {
    last_ = node;
  }
  document->IncrementTreeVersion();

  if (IsolateData::Get()->observer_registration_count) {
    QueueChildListMutation(isolate, this, node, nullptr, prev, child);
  }
  if (is_connected()) {
    // Siblings and their descendants can be affected through sibling
    // combinators and :nth-child(), the parent through :empty.
    InvalidateStyle(this);
    if (IsElement() && static_cast<Element*>(this)->HasTag("style")) {
      InvalidateAllStyles(document);
    }
    NotifyInserted(isolate, node);
  }
}

void ContainerNode::RemoveInternal(Isolate* isolate, Node* child) {
  DCHECK_EQ(child->parent(), this);
  if (child->is_connected()) {
    InvalidateStyle(child);
    InvalidateStyle(this);
    if (IsElement() && static_cast<Element*>(this)->HasTag("style")) {
      InvalidateAllStyles(child->document());
    }
  } else {
    // |child| may have been disconnected as part of an ancestor removal whose
    // style eviction is still deferred in dirty_roots; detaching it here would
    // hide it from DropStyles(ancestor), leaving dangling parent pointers.
    InvalidateStyleIfCached(child);
  }
  Node* prev = child->prev_.Get();
  Node* next = child->next_.Get();
  bool was_connected = child->is_connected();
  bool was_in_shadow_tree = was_connected && ContainingShadowRoot() != nullptr;
  if (prev) {
    prev->next_ = next;
  } else {
    first_ = next;
  }
  if (next) {
    next->prev_ = prev;
  } else {
    last_ = prev;
  }
  child->parent_ = nullptr;
  child->prev_ = nullptr;
  child->next_ = nullptr;
  child->document()->IncrementTreeVersion();

  if (IsolateData::Get()->observer_registration_count) {
    QueueChildListMutation(isolate, this, nullptr, child, prev, next);
  }
  if (was_connected) NotifyRemoved(isolate, child, was_in_shadow_tree);
}

bool ContainerNode::InsertBefore(Isolate* isolate, Node* node, Node* child,
                                 ExceptionState* exception_state) {
  if (!EnsurePreInsertionValidity(node, child, exception_state)) return false;
  CEReactionsScope reactions(isolate);
  if (child == node) child = node->next_sibling();
  if (node->IsFragment()) {
    ContainerNode* fragment = static_cast<ContainerNode*>(node);
    while (Node* moved = fragment->first()) {
      fragment->RemoveInternal(isolate, moved);
      InsertInternal(isolate, moved, child);
    }
    return true;
  }
  if (ContainerNode* old_parent = node->parent()) {
    old_parent->RemoveInternal(isolate, node);
  }
  InsertInternal(isolate, node, child);
  return true;
}

bool ContainerNode::RemoveChild(Isolate* isolate, Node* child,
                                ExceptionState* exception_state) {
  if (child->parent() != this) {
    return exception_state->Throw(
        "NotFoundError", "The node to be removed is not a child of this node.");
  }
  CEReactionsScope reactions(isolate);
  RemoveInternal(isolate, child);
  return true;
}

bool ContainerNode::ReplaceChild(Isolate* isolate, Node* node, Node* child,
                                 ExceptionState* exception_state) {
  if (child->parent() != this) {
    return exception_state->Throw(
        "NotFoundError",
        "The node to be replaced is not a child of this node.");
  }
  if (node == child) return true;
  // Removing |child| first makes the single-document-element check in the
  // validity algorithm behave as "replace" requires.
  if (node->ShadowIncludingContains(this)) {
    return exception_state->Throw("HierarchyRequestError",
                                  "The new child element contains the parent.");
  }
  CEReactionsScope reactions(isolate);
  Node* reference = child->next_sibling();
  if (reference == node) reference = node->next_sibling();
  RemoveInternal(isolate, child);
  return InsertBefore(isolate, node, reference, exception_state);
}

void ContainerNode::RemoveChildren(Isolate* isolate) {
  if (!first_) return;
  CEReactionsScope reactions(isolate);
  while (Node* child = first()) RemoveInternal(isolate, child);
}

void ContainerNode::ParserAppendChild(Isolate* isolate, Node* node) {
  DCHECK_NULL(node->parent());
  if (node->IsFragment()) {
    ContainerNode* fragment = static_cast<ContainerNode*>(node);
    while (Node* moved = fragment->first()) {
      fragment->RemoveInternal(isolate, moved);
      ParserAppendChild(isolate, moved);
    }
    return;
  }
  InsertInternal(isolate, node, nullptr);
}

void ContainerNode::ParserMoveChild(Isolate* isolate, Node* node) {
  if (ContainerNode* old_parent = node->parent()) {
    old_parent->RemoveInternal(isolate, node);
  }
  InsertInternal(isolate, node, nullptr);
}

Element* ContainerNode::FindElementById(const AtomicString& id) const {
  for (Node* n = first(); n; n = n->NextInPreOrder(this)) {
    if (n->IsElement() && static_cast<Element*>(n)->id() == id) {
      return static_cast<Element*>(n);
    }
  }
  return nullptr;
}

void ShadowRoot::Trace(Visitor* visitor) const {
  DocumentFragment::Trace(visitor);
  visitor->Trace(host_);
  visitor->Trace(adopted_style_sheets);
}

// --- Element -----------------------------------------------------------------

const char* NamespaceURI(Namespace ns) {
  switch (ns) {
    case Namespace::kHTML:
      return "http://www.w3.org/1999/xhtml";
    case Namespace::kSVG:
      return "http://www.w3.org/2000/svg";
    case Namespace::kMathML:
      return "http://www.w3.org/1998/Math/MathML";
    case Namespace::kNone:
      return nullptr;
  }
  UNREACHABLE();
}

Namespace NamespaceFromURI(const String& uri) {
  if (uri.is_null() || uri.empty()) return Namespace::kNone;
  if (uri.EqualsAscii(NamespaceURI(Namespace::kHTML))) return Namespace::kHTML;
  if (uri.EqualsAscii(NamespaceURI(Namespace::kSVG))) return Namespace::kSVG;
  if (uri.EqualsAscii(NamespaceURI(Namespace::kMathML))) {
    return Namespace::kMathML;
  }
  return Namespace::kNone;
}

void ElementRareData::Trace(Visitor* visitor) const {
  visitor->Trace(shadow_root);
  visitor->Trace(class_list);
  visitor->Trace(style);
  visitor->Trace(dataset);
  visitor->Trace(attributes);
  visitor->Trace(definition);
  visitor->Trace(template_content);
  visitor->Trace(canvas_context);
  visitor->Trace(content_window);
  visitor->Trace(sheet);
}

Element::Element(Document* document, Interface interface, Namespace ns,
                 AtomicString local_name)
    : ContainerNode(document, interface, kElementNode, 0),
      ns_(ns),
      local_name_(std::move(local_name)) {}

void Element::Trace(Visitor* visitor) const {
  ContainerNode::Trace(visitor);
  visitor->Trace(rare_data_);
}

AtomicString Element::TagName() const {
  if (ns_ != Namespace::kHTML || !local_name_.is_8bit()) return local_name_;
  // The upper-cased name is itself an atom, so repeated tagName reads return
  // the same string.
  return AtomicString(std::string_view(AsciiUpper(local_name_.view8())));
}

const AtomicString& Element::GetAttribute(const AtomicString& name) const {
  static const AtomicString* null_atom = new AtomicString();
  if (!rare_data_ || !rare_data_->style) {
    // Fast path: nothing to synchronise.
  } else {
    SynchronizeAttributes();
  }
  for (const Attribute& attribute : attributes_) {
    if (attribute.name == name) return attribute.value;
  }
  return *null_atom;
}

bool Element::HasAttribute(const AtomicString& name) const {
  return !GetAttribute(name).is_null();
}

void Element::SynchronizeAttributes() const {
  if (!rare_data_ || !rare_data_->style) return;
  CSSStyleDeclaration* style = rare_data_->style.Get();
  if (!style->attribute_is_stale()) return;
  // Inline style edits made through el.style are written back to the content
  // attribute lazily, the first time something reads it.
  AtomicString value(style->CssText());
  style->clear_attribute_is_stale();
  auto& attributes = const_cast<Element*>(this)->attributes_;
  static const AtomicString* style_atom = new AtomicString("style");
  for (Attribute& attribute : attributes) {
    if (attribute.name == *style_atom) {
      attribute.value = value;
      return;
    }
  }
  attributes.push_back({*style_atom, value});
}

void Element::UpdateClassNames(const AtomicString& value) {
  class_names_.clear();
  if (value.is_null() || !value.is_8bit()) {
    if (!value.is_null()) SplitOnHtmlSpace(value.ToUtf8(), &class_names_);
    return;
  }
  SplitOnHtmlSpace(value.view8(), &class_names_);
}

bool Element::HasClass(const AtomicString& name) const {
  for (const AtomicString& class_name : class_names_) {
    if (class_name == name) return true;
  }
  return false;
}

void Element::ParserSetAttribute(const AtomicString& name,
                                 const AtomicString& value) {
  for (Attribute& attribute : attributes_) {
    if (attribute.name == name) {
      AtomicString old = attribute.value;
      attribute.value = value;
      AttributeChanged(nullptr, name, old, value, true);
      return;
    }
  }
  attributes_.push_back({name, value});
  AttributeChanged(nullptr, name, AtomicString(), value, true);
}

void Element::SetAttribute(Isolate* isolate, const AtomicString& name,
                           const AtomicString& value) {
  SynchronizeAttributes();
  CEReactionsScope reactions(isolate);
  for (Attribute& attribute : attributes_) {
    if (attribute.name == name) {
      AtomicString old = attribute.value;
      attribute.value = value;
      AttributeChanged(isolate, name, old, value, false);
      return;
    }
  }
  attributes_.push_back({name, value});
  AttributeChanged(isolate, name, AtomicString(), value, false);
}

void Element::RemoveAttribute(Isolate* isolate, const AtomicString& name) {
  SynchronizeAttributes();
  for (size_t i = 0; i < attributes_.size(); ++i) {
    if (attributes_[i].name != name) continue;
    CEReactionsScope reactions(isolate);
    AtomicString old = attributes_[i].value;
    attributes_.erase(attributes_.begin() + i);
    AttributeChanged(isolate, name, old, AtomicString(), false);
    return;
  }
}

void Element::AttributeChanged(Isolate* isolate, const AtomicString& name,
                               const AtomicString& old_value,
                               const AtomicString& new_value,
                               bool from_parser) {
  static const AtomicString* id_atom = new AtomicString("id");
  static const AtomicString* class_atom = new AtomicString("class");
  static const AtomicString* style_atom = new AtomicString("style");
  static const AtomicString* src_atom = new AtomicString("src");
  static const AtomicString* href_atom = new AtomicString("href");

  if (name == *id_atom) {
    bool in_id_map = is_connected() && !ContainingShadowRoot();
    if (in_id_map && !id_.is_null()) document()->RemoveElementId(id_, this);
    id_ = new_value;
    if (in_id_map && !id_.is_null()) document()->AddElementId(id_, this);
  } else if (name == *class_atom) {
    UpdateClassNames(new_value);
  } else if (name == *style_atom) {
    if (rare_data_ && rare_data_->style) {
      rare_data_->style->SetCssTextFromAttribute(new_value);
    }
  }
  if (is_connected()) DidChangeState();
  if (from_parser) return;

  if (IsolateData::Get()->observer_registration_count) {
    QueueAttributeMutation(isolate, this, name, old_value);
  }
  if (has_flag(kIsCustom)) {
    CustomElementDefinition* def = definition();
    if (def && def->Observes(name)) {
      CEReactionsScope::Enqueue(isolate, this,
                                CEReactionsScope::kAttributeChanged, name,
                                old_value, new_value);
    }
  }
  if (is_connected() && (name == *src_atom || name == *href_atom) &&
      IsResourceElement(this)) {
    CEReactionsScope::Enqueue(isolate, this,
                              CEReactionsScope::kResourceChanged);
  }
}

AtomicString Element::NormalizeAttributeName(Isolate* isolate,
                                             Local<v8::String> name) const {
  AtomicString atom = ToAtomicString(isolate, name);
  if (ns_ != Namespace::kHTML || !atom.is_8bit()) return atom;
  std::string_view view = atom.view8();
  for (char c : view) {
    if (c >= 'A' && c <= 'Z')
      return AtomicString(std::string_view(AsciiLower(view)));
  }
  return atom;
}

ElementRareData* Element::EnsureRareData(Isolate* isolate) {
  if (!rare_data_) rare_data_ = MakeGarbageCollected<ElementRareData>(isolate);
  return rare_data_.Get();
}

ShadowRoot* Element::AttachShadow(Isolate* isolate, bool open) {
  ElementRareData* rare = EnsureRareData(isolate);
  DCHECK(!rare->shadow_root);
  rare->shadow_root =
      MakeGarbageCollected<ShadowRoot>(isolate, document(), this, open);
  rare->shadow_root->set_flag(kIsConnected, is_connected());
  if (is_connected()) DidChangeState();
  return rare->shadow_root.Get();
}

DocumentFragment* Element::TemplateContent(Isolate* isolate) {
  ElementRareData* rare = EnsureRareData(isolate);
  if (!rare->template_content) {
    rare->template_content = MakeGarbageCollected<DocumentFragment>(
        isolate, document()->TemplateContentsOwner(isolate));
    rare->template_content->set_flag(kIsTemplateContent, true);
  }
  return rare->template_content.Get();
}

// Form controls.

bool Element::IsFormControl() const {
  return IsHTML() &&
         (HasTag("input") || HasTag("textarea") || HasTag("select") ||
          HasTag("button") || HasTag("option") || HasTag("fieldset"));
}

bool Element::IsDisabled() const {
  static const AtomicString* disabled = new AtomicString("disabled");
  return IsFormControl() && HasAttribute(*disabled);
}

bool Element::IsCheckable() const {
  if (!HasTag("input")) return false;
  static const AtomicString* type = new AtomicString("type");
  const AtomicString& value = GetAttribute(*type);
  return !value.is_null() && value.is_8bit() &&
         (EqualsIgnoringAsciiCase(value.view8(), "checkbox") ||
          EqualsIgnoringAsciiCase(value.view8(), "radio"));
}

bool Element::Checked() const {
  if (rare_data_ && rare_data_->has_dirty_checked) return rare_data_->checked;
  static const AtomicString* checked = new AtomicString("checked");
  return HasAttribute(*checked);
}

void Element::SetChecked(Isolate* isolate, bool checked) {
  ElementRareData* rare = EnsureRareData(isolate);
  rare->checked = checked;
  rare->has_dirty_checked = true;
  DidChangeState();
}

bool Element::Selected() const {
  if (rare_data_ && rare_data_->has_dirty_selected) return rare_data_->selected;
  static const AtomicString* selected = new AtomicString("selected");
  return HasAttribute(*selected);
}

namespace {

Element* SelectedOption(const Element* select) {
  Element* first_option = nullptr;
  for (Node* n = select->first(); n; n = n->NextInPreOrder(select)) {
    if (!n->IsElement()) continue;
    Element* element = static_cast<Element*>(n);
    if (!element->HasTag("option")) continue;
    if (!first_option) first_option = element;
    if (element->Selected()) return element;
  }
  return first_option;
}

}  // namespace

String Element::Value() const {
  static const AtomicString* value_atom = new AtomicString("value");
  if (HasTag("select")) {
    Element* option = SelectedOption(this);
    return option ? option->Value() : String::Empty();
  }
  if (HasTag("option")) {
    const AtomicString& value = GetAttribute(*value_atom);
    return value.is_null() ? TextContent() : String(value);
  }
  if (rare_data_ && rare_data_->has_dirty_value) return rare_data_->value;
  if (HasTag("textarea")) return TextContent();
  const AtomicString& value = GetAttribute(*value_atom);
  if (!value.is_null()) return value;
  return IsCheckable() ? String(AtomicString("on")) : String::Empty();
}

void Element::SetValue(Isolate* isolate, const String& value) {
  if (HasTag("select")) {
    for (Node* n = first(); n; n = n->NextInPreOrder(this)) {
      if (!n->IsElement()) continue;
      Element* element = static_cast<Element*>(n);
      if (!element->HasTag("option")) continue;
      ElementRareData* rare = element->EnsureRareData(isolate);
      rare->selected = element->Value() == value;
      rare->has_dirty_selected = true;
    }
    return;
  }
  ElementRareData* rare = EnsureRareData(isolate);
  rare->value = value.is_null() ? String::Empty() : value;
  rare->has_dirty_value = true;
  DidChangeState();
}

void Element::DidChangeState() {
  // A change to this element can restyle its descendants and, through
  // sibling combinators, its following siblings.
  ContainerNode* container = parent();
  InvalidateStyle(container ? static_cast<Node*>(container) : this);
  document()->InvalidateLayout();
}

void Document::set_focused_element(Element* element) {
  Element* old = focused_element_.Get();
  if (old == element) return;
  focused_element_ = element;
  if (old && old->is_connected()) old->DidChangeState();
  if (element) element->DidChangeState();
}

// Focus.

namespace {

// Only elements that are being rendered can be focused: focus() on an
// element inside a display: none subtree does nothing.
bool IsBeingRendered(Element* element) {
  for (Node* node = element; node && node->IsElement();) {
    const ComputedStyle* style = ComputedStyleFor(static_cast<Element*>(node));
    if (!style || style->display == ComputedStyle::Display::kNone) return false;
    ContainerNode* parent = node->parent();
    if (parent && parent->IsShadowRoot()) {
      node = static_cast<ShadowRoot*>(parent)->host();
    } else {
      node = parent;
    }
  }
  return true;
}

bool IsFocusable(Element* element) {
  static const AtomicString* tabindex = new AtomicString("tabindex");
  static const AtomicString* contenteditable =
      new AtomicString("contenteditable");
  static const AtomicString* href = new AtomicString("href");
  if (!element->is_connected()) return false;
  if (element->IsDisabled()) return false;
  if (!IsBeingRendered(element)) return false;
  if (element->HasAttribute(*tabindex)) return true;
  const AtomicString& editable = element->GetAttribute(*contenteditable);
  if (!editable.is_null() && !editable.EqualsAscii("false")) return true;
  if (!element->IsHTML()) return false;
  return element->HasTag("input") || element->HasTag("textarea") ||
         element->HasTag("select") || element->HasTag("button") ||
         element->HasTag("iframe") || element->HasTag("summary") ||
         (element->HasTag("a") && element->HasAttribute(*href));
}

void FireFocusEvent(Isolate* isolate, Element* target, const char* type,
                    bool bubbles, Element* related) {
  Event* event = MakeGarbageCollected<Event>(
      isolate, isolate, Interface::kFocusEvent, AtomicString(type));
  event->bubbles = bubbles;
  event->composed = true;
  event->is_trusted = true;
  event->target_slots[event->FieldIndex("relatedTarget")] = related;
  target->DispatchEvent(isolate, event);
}

}  // namespace

void Element::Focus(Isolate* isolate) {
  if (!IsFocusable(this)) return;
  Document* doc = document();
  Element* old = doc->focused_element();
  if (old == this) return;
  doc->set_focused_element(this);
  if (old) {
    FireFocusEvent(isolate, old, "blur", false, this);
    FireFocusEvent(isolate, old, "focusout", true, this);
  }
  if (doc->focused_element() != this) return;
  FireFocusEvent(isolate, this, "focus", false, old);
  FireFocusEvent(isolate, this, "focusin", true, old);
}

void Element::Blur(Isolate* isolate) {
  Document* doc = document();
  if (doc->focused_element() != this) return;
  doc->set_focused_element(nullptr);
  FireFocusEvent(isolate, this, "blur", false, nullptr);
  FireFocusEvent(isolate, this, "focusout", true, nullptr);
}

void Element::Click(Isolate* isolate) {
  if (IsDisabled()) return;
  Event* event = MakeGarbageCollected<Event>(
      isolate, isolate, Interface::kPointerEvent, AtomicString("click"));
  event->bubbles = true;
  event->cancelable = true;
  event->composed = true;
  DispatchEvent(isolate, event);
}

// --- Event dispatch through the tree ----------------------------------------

namespace {

void FireSimpleEvent(Isolate* isolate, EventTarget* target, const char* type,
                     bool bubbles, bool cancelable = false) {
  Event* event = MakeGarbageCollected<Event>(
      isolate, isolate, Interface::kEvent, AtomicString(type));
  event->bubbles = bubbles;
  event->cancelable = cancelable;
  event->is_trusted = true;
  target->DispatchEvent(isolate, event);
}

Element* FormOwner(Element* control) {
  for (Element* e = control->parent_element(); e; e = e->parent_element()) {
    if (e->HasTag("form")) return e;
  }
  return nullptr;
}

bool InputTypeIs(Element* element, std::string_view type) {
  static const AtomicString* type_atom = new AtomicString("type");
  const AtomicString& value = element->GetAttribute(*type_atom);
  if (value.is_null()) return type == "text";
  return value.is_8bit() && EqualsIgnoringAsciiCase(value.view8(), type);
}

bool IsSubmitButton(Element* element) {
  static const AtomicString* type_atom = new AtomicString("type");
  if (element->HasTag("button")) {
    const AtomicString& type = element->GetAttribute(*type_atom);
    return type.is_null() || !type.is_8bit() ||
           (!EqualsIgnoringAsciiCase(type.view8(), "button") &&
            !EqualsIgnoringAsciiCase(type.view8(), "reset"));
  }
  return element->HasTag("input") && InputTypeIs(element, "submit");
}

Element* LabeledControl(Element* label) {
  static const AtomicString* for_atom = new AtomicString("for");
  const AtomicString& target_id = label->GetAttribute(*for_atom);
  if (!target_id.is_null()) {
    Node* root = label->Root();
    if (root->IsDocument()) {
      return static_cast<Document*>(root)->GetElementById(target_id);
    }
    return static_cast<ContainerNode*>(root)->FindElementById(target_id);
  }
  for (Node* n = label->first(); n; n = n->NextInPreOrder(label)) {
    if (!n->IsElement()) continue;
    Element* element = static_cast<Element*>(n);
    if (element->HasTag("input") || element->HasTag("textarea") ||
        element->HasTag("select") || element->HasTag("button")) {
      return element;
    }
  }
  return nullptr;
}

// Whether a click on |element| has activation behaviour.
bool HasActivationBehavior(Element* element) {
  static const AtomicString* href = new AtomicString("href");
  if (!element->IsHTML()) return false;
  if (element->HasTag("input")) {
    return element->IsCheckable() || InputTypeIs(element, "submit");
  }
  return element->HasTag("button") || element->HasTag("label") ||
         (element->HasTag("a") && element->HasAttribute(*href));
}

void UncheckOtherRadios(Isolate* isolate, Element* radio) {
  static const AtomicString* name_atom = new AtomicString("name");
  const AtomicString& name = radio->GetAttribute(*name_atom);
  if (name.is_null() || name.empty()) return;
  Node* scope = FormOwner(radio);
  if (!scope) scope = radio->Root();
  for (Node* n = scope->first_child(); n; n = n->NextInPreOrder(scope)) {
    if (n == radio || !n->IsElement()) continue;
    Element* other = static_cast<Element*>(n);
    if (other->HasTag("input") && InputTypeIs(other, "radio") &&
        other->GetAttribute(*name_atom) == name) {
      other->SetChecked(isolate, false);
    }
  }
}

void RunActivationBehavior(Isolate* isolate, Element* element, Event* event) {
  static const AtomicString* href_atom = new AtomicString("href");
  if (element->HasTag("input") && element->IsCheckable()) {
    FireSimpleEvent(isolate, element, "input", true);
    FireSimpleEvent(isolate, element, "change", true);
    return;
  }
  if (IsSubmitButton(element)) {
    Element* form = FormOwner(element);
    if (!form) return;
    Event* submit = MakeGarbageCollected<Event>(
        isolate, isolate, Interface::kSubmitEvent, AtomicString("submit"));
    submit->bubbles = true;
    submit->cancelable = true;
    submit->is_trusted = true;
    submit->target_slots[submit->FieldIndex("submitter")] = element;
    if (form->DispatchEvent(isolate, submit)) {
      form->document()->window()->ReportUnsupported(
          isolate, "form submission that navigates");
    }
    return;
  }
  if (element->HasTag("label")) {
    Element* control = LabeledControl(element);
    Node* target = event->target ? event->target->AsNode() : nullptr;
    if (control && !(target && control->Contains(target))) {
      control->Click(isolate);
    }
    return;
  }
  if (element->HasTag("a")) {
    const AtomicString& href = element->GetAttribute(*href_atom);
    element->document()->window()->location()->FollowHyperlink(isolate, href);
    return;
  }
}

}  // namespace

bool Node::DispatchNodeEvent(Isolate* isolate, Event* event) {
  struct PathEntry {
    CPPGC_STACK_ALLOCATED();

   public:
    EventTarget* target;
    // What event.target reads as while this entry's listeners run.
    EventTarget* retargeted;
    bool is_target;
  };
  // Everything on the path is kept alive by event->path (set below) or, for
  // the window, by its realm.
  MOCK_DOM_LOCAL_NODES
  std::vector<PathEntry> path;
  MOCK_DOM_LOCAL_NODES
  std::vector<Node*> path_nodes;
  {
    Node* node = this;
    EventTarget* retargeted = this;
    bool is_target = true;
    while (true) {
      path.push_back({node, retargeted, is_target});
      path_nodes.push_back(node);
      is_target = false;
      if (ContainerNode* parent = node->parent()) {
        node = parent;
        continue;
      }
      if (node->IsShadowRoot()) {
        if (!event->composed) break;
        // Outside the shadow tree the event appears to come from the host.
        node = static_cast<ShadowRoot*>(node)->host();
        retargeted = node;
        is_target = true;
        continue;
      }
      if (node->IsDocument()) {
        Document* document = static_cast<Document*>(node);
        static const AtomicString* load = new AtomicString("load");
        if (document->default_view() && event->type() != *load) {
          path.push_back({document->window(), retargeted, false});
        }
      }
      break;
    }
  }
  // The path list keeps every node on the path alive while listeners run and
  // backs composedPath().
  event->path = NodeList::CreateStatic(isolate, path_nodes);

  // Activation behaviour, as in the DOM "dispatch" algorithm: find the
  // nearest activatable element on the path for click events.
  static const AtomicString* click = new AtomicString("click");
  Element* activation_target = nullptr;
  bool pre_activation_checked = false;
  bool ran_pre_activation = false;
  if (event->type() == *click &&
      InterfaceInheritsFrom(event->interface(), Interface::kMouseEvent)) {
    for (const PathEntry& entry : path) {
      Node* node = entry.target->AsNode();
      if (!node || !node->IsElement()) continue;
      Element* element = static_cast<Element*>(node);
      if (HasActivationBehavior(element)) {
        activation_target = element;
        break;
      }
      if (!event->bubbles) break;
    }
    if (activation_target && activation_target->IsCheckable() &&
        !activation_target->IsDisabled()) {
      // Legacy pre-activation: the checkbox flips before listeners run, and
      // flips back if the event gets cancelled.
      ran_pre_activation = true;
      pre_activation_checked = activation_target->Checked();
      if (InputTypeIs(activation_target, "radio")) {
        activation_target->SetChecked(isolate, true);
        UncheckOtherRadios(isolate, activation_target);
      } else {
        activation_target->SetChecked(isolate, !pre_activation_checked);
      }
    }
  }

  event->dispatching = true;
  event->target = this;
  for (size_t i = path.size(); i-- > 0;) {
    const PathEntry& entry = path[i];
    event->target = entry.retargeted;
    event->current_target = entry.target;
    event->phase = entry.is_target ? Event::kAtTarget : Event::kCapturing;
    entry.target->FireListeners(isolate, event, true, entry.is_target);
    if (event->propagation_stopped) break;
  }
  if (!event->propagation_stopped) {
    for (const PathEntry& entry : path) {
      if (!entry.is_target && !event->bubbles) continue;
      event->target = entry.retargeted;
      event->current_target = entry.target;
      event->phase = entry.is_target ? Event::kAtTarget : Event::kBubbling;
      entry.target->FireListeners(isolate, event, false, entry.is_target);
      if (event->propagation_stopped) break;
    }
  }
  event->target = path.back().retargeted;
  event->phase = Event::kNone;
  event->current_target = nullptr;
  event->dispatching = false;
  event->propagation_stopped = false;
  event->immediate_propagation_stopped = false;

  if (activation_target && !activation_target->IsDisabled()) {
    if (!event->default_prevented) {
      RunActivationBehavior(isolate, activation_target, event);
    } else if (ran_pre_activation) {
      activation_target->SetChecked(isolate, pre_activation_checked);
    }
  }
  return !event->default_prevented;
}

// --- Document ----------------------------------------------------------------

namespace {

struct TagInterface {
  const char* tag;
  Interface interface;
};

const TagInterface kHTMLTags[] = {
    {"html", Interface::kHTMLHtmlElement},
    {"head", Interface::kHTMLHeadElement},
    {"body", Interface::kHTMLBodyElement},
    {"title", Interface::kHTMLTitleElement},
    {"meta", Interface::kHTMLMetaElement},
    {"link", Interface::kHTMLLinkElement},
    {"style", Interface::kHTMLStyleElement},
    {"script", Interface::kHTMLScriptElement},
    {"div", Interface::kHTMLDivElement},
    {"span", Interface::kHTMLSpanElement},
    {"p", Interface::kHTMLParagraphElement},
    {"h1", Interface::kHTMLHeadingElement},
    {"h2", Interface::kHTMLHeadingElement},
    {"h3", Interface::kHTMLHeadingElement},
    {"h4", Interface::kHTMLHeadingElement},
    {"h5", Interface::kHTMLHeadingElement},
    {"h6", Interface::kHTMLHeadingElement},
    {"pre", Interface::kHTMLPreElement},
    {"br", Interface::kHTMLBRElement},
    {"hr", Interface::kHTMLHRElement},
    {"a", Interface::kHTMLAnchorElement},
    {"img", Interface::kHTMLImageElement},
    {"ul", Interface::kHTMLUListElement},
    {"ol", Interface::kHTMLOListElement},
    {"li", Interface::kHTMLLIElement},
    {"dl", Interface::kHTMLDListElement},
    {"input", Interface::kHTMLInputElement},
    {"button", Interface::kHTMLButtonElement},
    {"label", Interface::kHTMLLabelElement},
    {"form", Interface::kHTMLFormElement},
    {"fieldset", Interface::kHTMLFieldSetElement},
    {"select", Interface::kHTMLSelectElement},
    {"option", Interface::kHTMLOptionElement},
    {"textarea", Interface::kHTMLTextAreaElement},
    {"table", Interface::kHTMLTableElement},
    {"thead", Interface::kHTMLTableSectionElement},
    {"tbody", Interface::kHTMLTableSectionElement},
    {"tfoot", Interface::kHTMLTableSectionElement},
    {"tr", Interface::kHTMLTableRowElement},
    {"td", Interface::kHTMLTableCellElement},
    {"th", Interface::kHTMLTableCellElement},
    {"canvas", Interface::kHTMLCanvasElement},
    {"template", Interface::kHTMLTemplateElement},
    {"slot", Interface::kHTMLSlotElement},
    {"iframe", Interface::kHTMLIFrameElement},
    {"time", Interface::kHTMLTimeElement},
    {"picture", Interface::kHTMLPictureElement},
    {"source", Interface::kHTMLSourceElement},
    {"details", Interface::kHTMLDetailsElement},
    {"dialog", Interface::kHTMLDialogElement},
};

// Elements that use the plain HTMLElement interface.
const char* const kPlainHTMLTags[] = {
    "abbr",     "address",    "article",    "aside",   "b",        "bdi",
    "bdo",      "cite",       "code",       "dd",      "dfn",      "dt",
    "em",       "figcaption", "figure",     "footer",  "header",   "hgroup",
    "i",        "kbd",        "main",       "mark",    "nav",      "noscript",
    "rp",       "rt",         "ruby",       "s",       "samp",     "section",
    "small",    "strong",     "sub",        "summary", "sup",      "u",
    "var",      "wbr",        "search",     "center",  "nobr",     "tt",
    "big",      "strike",     "blockquote", "q",       "caption",  "col",
    "colgroup", "legend",     "optgroup",   "output",  "progress", "meter",
    "datalist", "menu",       "ins",        "del",     "area",     "map",
    "audio",    "video",      "track",      "embed",   "object",   "param",
    "base",     "data",
};

const TagInterface kSVGTags[] = {
    {"svg", Interface::kSVGSVGElement},
    {"g", Interface::kSVGGElement},
    {"defs", Interface::kSVGDefsElement},
    {"use", Interface::kSVGUseElement},
    {"image", Interface::kSVGImageElement},
    {"foreignObject", Interface::kSVGForeignObjectElement},
    {"path", Interface::kSVGPathElement},
    {"rect", Interface::kSVGRectElement},
    {"circle", Interface::kSVGCircleElement},
    {"ellipse", Interface::kSVGEllipseElement},
    {"line", Interface::kSVGLineElement},
    {"polyline", Interface::kSVGPolylineElement},
    {"polygon", Interface::kSVGPolygonElement},
    {"text", Interface::kSVGTextElement},
    {"tspan", Interface::kSVGTSpanElement},
    {"clipPath", Interface::kSVGClipPathElement},
    {"linearGradient", Interface::kSVGLinearGradientElement},
    {"stop", Interface::kSVGStopElement},
    {"title", Interface::kSVGTitleElement},
};

using TagMap = absl::flat_hash_map<AtomicString, Interface, AtomicString::Hash>;

Interface InterfaceForHTMLTag(const AtomicString& local_name) {
  static const TagMap* map = [] {
    auto* m = new TagMap();
    for (const TagInterface& entry : kHTMLTags) {
      m->emplace(AtomicString(entry.tag), entry.interface);
    }
    for (const char* tag : kPlainHTMLTags) {
      m->emplace(AtomicString(tag), Interface::kHTMLElement);
    }
    return m;
  }();
  auto it = map->find(local_name);
  if (it != map->end()) return it->second;
  return Interface::kNone;
}

Interface InterfaceForSVGTag(const AtomicString& local_name) {
  static const TagMap* map = [] {
    auto* m = new TagMap();
    for (const TagInterface& entry : kSVGTags) {
      m->emplace(AtomicString(entry.tag), entry.interface);
    }
    return m;
  }();
  auto it = map->find(local_name);
  return it != map->end() ? it->second : Interface::kSVGElement;
}

}  // namespace

Document::Document(Isolate* isolate, Window* window)
    : ContainerNode(nullptr, Interface::kDocument, kDocumentNode, kIsConnected),
      window_(window) {}

void Document::Trace(Visitor* visitor) const {
  ContainerNode::Trace(visitor);
  visitor->Trace(window_);
  visitor->Trace(focused_element_);
  visitor->Trace(template_contents_owner_);
  visitor->Trace(current_script);
  visitor->Trace(selection);
  visitor->Trace(adopted_style_sheets);
}

Element* Document::document_element() const { return first_element_child(); }

Element* Document::body() const {
  Element* html = document_element();
  if (!html) return nullptr;
  for (Node* n = html->first(); n; n = n->next_sibling()) {
    if (n->IsElement() && (static_cast<Element*>(n)->HasTag("body") ||
                           static_cast<Element*>(n)->HasTag("frameset"))) {
      return static_cast<Element*>(n);
    }
  }
  return nullptr;
}

Element* Document::head() const {
  Element* html = document_element();
  if (!html) return nullptr;
  for (Node* n = html->first(); n; n = n->next_sibling()) {
    if (n->IsElement() && static_cast<Element*>(n)->HasTag("head")) {
      return static_cast<Element*>(n);
    }
  }
  return nullptr;
}

Element* Document::CreateElement(Isolate* isolate, Namespace ns,
                                 const AtomicString& local_name) {
  Interface interface = Interface::kElement;
  uint16_t flags = 0;
  if (ns == Namespace::kHTML) {
    interface = InterfaceForHTMLTag(local_name);
    if (interface == Interface::kNone) {
      if (CustomElementRegistry::IsValidName(local_name)) {
        interface = Interface::kHTMLElement;
        flags = kIsUndefinedCustom;
      } else {
        interface = Interface::kHTMLUnknownElement;
      }
    }
  } else if (ns == Namespace::kSVG) {
    interface = InterfaceForSVGTag(local_name);
  }
  Element* element =
      MakeGarbageCollected<Element>(isolate, this, interface, ns, local_name);
  if (flags) element->set_flag(kIsUndefinedCustom, true);
  return element;
}

Node* Document::CreateTextNode(Isolate* isolate, const String& data) {
  return MakeGarbageCollected<CharacterData>(
      isolate, this, Interface::kText, kTextNode,
      data.is_null() ? String::Empty() : data);
}

Node* Document::CreateComment(Isolate* isolate, const String& data) {
  return MakeGarbageCollected<CharacterData>(
      isolate, this, Interface::kComment, kCommentNode,
      data.is_null() ? String::Empty() : data);
}

DocumentFragment* Document::CreateDocumentFragment(Isolate* isolate) {
  return MakeGarbageCollected<DocumentFragment>(isolate, this);
}

Element* Document::GetElementById(const AtomicString& id) const {
  auto it = ids_.find(id);
  if (it == ids_.end() || it->second.empty()) return nullptr;
  if (it->second.size() == 1) return it->second[0].Get();
  // Duplicate ids: the first in tree order wins.
  Element* best = it->second[0].Get();
  for (size_t i = 1; i < it->second.size(); ++i) {
    Element* candidate = it->second[i].Get();
    if (candidate->CompareDocumentPosition(best) & 4) best = candidate;
  }
  return best;
}

void Document::AddElementId(const AtomicString& id, Element* element) {
  ids_[id].emplace_back(element);
}

void Document::RemoveElementId(const AtomicString& id, Element* element) {
  auto it = ids_.find(id);
  if (it == ids_.end()) return;
  auto& elements = it->second;
  for (size_t i = 0; i < elements.size(); ++i) {
    if (elements[i].Get() == element) {
      elements.erase(elements.begin() + i);
      break;
    }
  }
  if (elements.empty()) ids_.erase(it);
}

Element* Document::active_element() const {
  if (focused_element_) return focused_element_.Get();
  return body();
}

Document* Document::TemplateContentsOwner(Isolate* isolate) {
  if (!has_browsing_context_) return this;
  if (!template_contents_owner_) {
    template_contents_owner_ =
        MakeGarbageCollected<Document>(isolate, isolate, window());
  }
  return template_contents_owner_.Get();
}

// --- NodeList ----------------------------------------------------------------

void NodeList::Trace(Visitor* visitor) const {
  if (DeferTraceIfConcurrent(visitor, this)) return;
  ScriptWrappable::Trace(visitor);
  visitor->Trace(root_);
  for (const auto& item : items_) visitor->Trace(item);
}

Window* NodeList::WrapperWindow() const {
  if (root_) return root_->WrapperWindow();
  if (!items_.empty()) return items_[0]->WrapperWindow();
  return nullptr;
}

NodeList* NodeList::CreateStatic(Isolate* isolate,
                                 const std::vector<Node*>& nodes) {
  NodeList* list = MakeGarbageCollected<NodeList>(
      isolate, Interface::kNodeList, kStatic, nullptr, AtomicString());
  list->items_.reserve(nodes.size());
  for (Node* node : nodes) list->items_.emplace_back(node);
  return list;
}

NodeList* NodeList::CreateStatic(Isolate* isolate,
                                 const std::vector<Element*>& nodes) {
  NodeList* list = MakeGarbageCollected<NodeList>(
      isolate, Interface::kNodeList, kStatic, nullptr, AtomicString());
  list->items_.reserve(nodes.size());
  for (Element* node : nodes) list->items_.emplace_back(node);
  return list;
}

void NodeList::SetClassNames(std::string_view names) {
  SplitOnHtmlSpace(names, &class_names_);
}

void NodeList::Update() {
  if (kind_ == kStatic) return;
  Document* document = root_->IsDocument() ? static_cast<Document*>(root_.Get())
                                           : root_->document();
  uint64_t version = document->tree_version();
  if (cached_version_ == version) return;
  cached_version_ = version;
  items_.clear();
  switch (kind_) {
    case kStatic:
      break;
    case kChildNodes:
      for (Node* n = root_->first_child(); n; n = n->next_sibling()) {
        items_.emplace_back(n);
      }
      break;
    case kChildren:
      for (Node* n = root_->first_child(); n; n = n->next_sibling()) {
        if (n->IsElement()) items_.emplace_back(n);
      }
      break;
    case kByTagName: {
      bool any = name_.EqualsAscii("*");
      // HTML elements match case-insensitively; their local names are
      // lower-case, so compare against the lower-cased query.
      AtomicString lower =
          name_.is_8bit()
              ? AtomicString(std::string_view(AsciiLower(name_.view8())))
              : name_;
      for (Node* n = root_->first_child(); n; n = n->NextInPreOrder(root_)) {
        if (!n->IsElement()) continue;
        Element* element = static_cast<Element*>(n);
        if (any ||
            element->local_name() == (element->IsHTML() ? lower : name_)) {
          items_.emplace_back(n);
        }
      }
      break;
    }
    case kByClassName:
      if (class_names_.empty()) break;
      for (Node* n = root_->first_child(); n; n = n->NextInPreOrder(root_)) {
        if (!n->IsElement()) continue;
        Element* element = static_cast<Element*>(n);
        bool all = true;
        for (const AtomicString& name : class_names_) {
          if (!element->HasClass(name)) {
            all = false;
            break;
          }
        }
        if (all) items_.emplace_back(n);
      }
      break;
  }
}

uint32_t NodeList::length() {
  Update();
  return static_cast<uint32_t>(items_.size());
}

Node* NodeList::item(uint32_t index) {
  Update();
  return index < items_.size() ? items_[index].Get() : nullptr;
}

uint32_t NodeList::IndexOf(Node* needle, uint32_t start_index,
                           uint32_t end_index, uint32_t* out_length) {
  Update();
  uint32_t length = static_cast<uint32_t>(items_.size());
  *out_length = length;
  if (!needle) return std::numeric_limits<uint32_t>::max();
  uint32_t end = std::min(end_index, length);
  for (uint32_t i = start_index; i < end; ++i) {
    if (items_[i].Get() == needle) return i;
  }
  return std::numeric_limits<uint32_t>::max();
}

void SplitOnHtmlSpace(std::string_view text, std::vector<AtomicString>* out) {
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && IsHtmlSpace(text[i])) ++i;
    size_t start = i;
    while (i < text.size() && !IsHtmlSpace(text[i])) ++i;
    if (i > start)
      out->push_back(AtomicString::FromUtf8(text.substr(start, i - start)));
  }
}

// --- Window ------------------------------------------------------------------

namespace {
constexpr EmbedderDataTypeTag kWindowTag = 2;
}  // namespace

Window::Window(Isolate* isolate, Local<Context> context)
    : EventTarget(Interface::kWindow) {
  context_.Reset(isolate, context);
}

void Window::Initialize(Isolate* isolate) {
  document_ = MakeGarbageCollected<Document>(isolate, isolate, this);
  document_->set_has_browsing_context(true);
  custom_elements_ = MakeGarbageCollected<CustomElementRegistry>(isolate, this);
  location_ = MakeGarbageCollected<Location>(isolate, this);
  history_ = MakeGarbageCollected<History>(isolate, this);
  local_storage_ = MakeGarbageCollected<Storage>(isolate, this);
  session_storage_ = MakeGarbageCollected<Storage>(isolate, this);
}

void Window::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  visitor->Trace(context_);
  visitor->Trace(document_);
  visitor->Trace(custom_elements_);
  visitor->Trace(location_);
  visitor->Trace(history_);
  visitor->Trace(local_storage_);
  visitor->Trace(session_storage_);
  visitor->Trace(indexed_db);
  visitor->Trace(resource_callback);
  visitor->Trace(frame_element);
}

Window* Window::From(Local<Context> context) {
  if (context->GetNumberOfEmbedderDataFields() <=
      static_cast<uint32_t>(kWindowEmbedderDataIndex)) {
    return nullptr;
  }
  return static_cast<Window*>(context->GetAlignedPointerFromEmbedderData(
      kWindowEmbedderDataIndex, kWindowTag));
}

void Window::Install(Isolate* isolate, Local<Context> context,
                     Local<Object> holder, Window* window) {
  // The holder object wraps the window and lives in the context's embedder
  // data, so the window lives exactly as long as its realm.
  context->SetEmbedderDataV3(kWindowHolderEmbedderDataIndex, holder);
  context->SetAlignedPointerInEmbedderDataV2(kWindowEmbedderDataIndex, window,
                                             kWindowTag);
}

void Window::ReportException(Isolate* isolate, Local<Value> exception,
                             Local<Message> message) {
  IsolateData* data = IsolateData::Get();
  HandleScope handle_scope(isolate);
  Local<Context> context = this->context(isolate);

  std::string text = "Uncaught exception";
  std::string filename;
  int line = 0;
  int column = 0;
  if (!message.IsEmpty()) {
    text = ToUtf8(isolate, message->Get());
    Local<Value> resource = message->GetScriptResourceName();
    if (!resource.IsEmpty() && resource->IsString()) {
      filename = ToUtf8(isolate, resource.As<v8::String>());
    }
    line = message->GetLineNumber(context).FromMaybe(0);
    column = message->GetStartColumn(context).FromMaybe(0);
  }

  bool handled = false;
  if (!data->reporting_exception) {
    // An exception thrown by an error listener is not reported again.
    data->reporting_exception = true;
    Event* event = MakeGarbageCollected<Event>(
        isolate, isolate, Interface::kErrorEvent, AtomicString("error"));
    event->cancelable = true;
    event->is_trusted = true;
    event->set_string("message", String::FromUtf8(text));
    event->set_string("filename", String::FromUtf8(filename));
    event->set_number("lineno", line);
    event->set_number("colno", column);
    event->any_slots[event->FieldIndex("error")].Reset(isolate, exception);
    handled = !DispatchEvent(isolate, event);
    data->reporting_exception = false;
  }
  if (handled) return;

  ++data->uncaught_error_count;
  std::string stack;
  if (exception->IsObject()) {
    TryCatch try_catch(isolate);
    Local<Value> stack_value;
    if (exception.As<Object>()
            ->Get(context, v8::String::NewFromUtf8Literal(isolate, "stack"))
            .ToLocal(&stack_value) &&
        stack_value->IsString()) {
      stack = ToUtf8(isolate, stack_value.As<v8::String>());
    }
  }
  fprintf(stderr, "[mock-dom] %s\n    at %s:%d:%d\n", text.c_str(),
          filename.c_str(), line, column);
  if (!stack.empty()) fprintf(stderr, "%s\n", stack.c_str());
}

void Window::ReportUnsupported(Isolate* isolate, const char* what) {
  // Reaching a browser feature the mock does not implement must never pass
  // silently: surface it the same way as an uncaught exception.
  HandleScope handle_scope(isolate);
  std::string text = std::string("mock-dom: unsupported: ") + what;
  Local<Value> error = Exception::Error(ToV8StringFromUtf8(isolate, text));
  ReportException(isolate, error, Exception::CreateMessage(isolate, error));
}

// --- Custom elements ---------------------------------------------------------

void CustomElementDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(constructor);
  visitor->Trace(connected_callback);
  visitor->Trace(disconnected_callback);
  visitor->Trace(adopted_callback);
  visitor->Trace(attribute_changed_callback);
  visitor->Trace(next);
}

bool CustomElementDefinition::Observes(const AtomicString& attribute) const {
  for (const AtomicString& observed : observed_attributes) {
    if (observed == attribute) return true;
  }
  return false;
}

void CustomElementRegistry::Trace(Visitor* visitor) const {
  if (DeferTraceIfConcurrent(visitor, this)) return;
  ScriptWrappable::Trace(visitor);
  visitor->Trace(window_);
  visitor->Trace(definitions_);
  for (const auto& entry : when_defined_) visitor->Trace(entry.second);
}

bool CustomElementRegistry::IsValidName(const AtomicString& name) {
  if (name.is_null() || !name.is_8bit()) return false;
  std::string_view view = name.view8();
  if (view.empty() || view[0] < 'a' || view[0] > 'z') return false;
  bool has_dash = false;
  for (char c : view) {
    if (c >= 'A' && c <= 'Z') return false;
    if (c == '-') has_dash = true;
  }
  if (!has_dash) return false;
  static const char* const kReserved[] = {
      "annotation-xml", "color-profile",    "font-face",      "font-face-src",
      "font-face-uri",  "font-face-format", "font-face-name", "missing-glyph"};
  for (const char* reserved : kReserved) {
    if (view == reserved) return false;
  }
  return true;
}

CustomElementDefinition* CustomElementRegistry::Lookup(
    const AtomicString& name) const {
  for (CustomElementDefinition* d = definitions_; d; d = d->next) {
    if (d->name == name) return d;
  }
  return nullptr;
}

CustomElementDefinition* CustomElementRegistry::LookupByConstructor(
    Isolate* isolate, Local<Value> constructor) const {
  for (CustomElementDefinition* d = definitions_; d; d = d->next) {
    if (d->constructor.Get(isolate)->StrictEquals(constructor)) return d;
  }
  return nullptr;
}

namespace {

bool GetCallback(Isolate* isolate, Local<Context> context,
                 Local<Object> prototype, const char* name,
                 TracedReference<Function>* out,
                 ExceptionState* exception_state) {
  Local<Value> value;
  if (!prototype->Get(context, ToV8StringFromUtf8(isolate, name))
           .ToLocal(&value)) {
    return false;
  }
  if (value->IsUndefined()) return true;
  if (!value->IsFunction()) {
    return exception_state->Throw(
        "TypeError", std::string("The callback provided as ") + name +
                         " is not a function.");
  }
  out->Reset(isolate, value.As<Function>());
  return true;
}

}  // namespace

void CustomElementRegistry::Define(Isolate* isolate, const AtomicString& name,
                                   Local<Function> constructor,
                                   ExceptionState* exception_state) {
  Local<Context> context = isolate->GetCurrentContext();
  if (!constructor->IsConstructor()) {
    exception_state->Throw("TypeError",
                           "The provided value is not a constructor.");
    return;
  }
  if (!IsValidName(name)) {
    exception_state->Throw("SyntaxError", "\"" + name.ToUtf8() +
                                              "\" is not a valid custom "
                                              "element name");
    return;
  }
  if (Lookup(name)) {
    exception_state->Throw("NotSupportedError",
                           "the name \"" + name.ToUtf8() +
                               "\" has already been used with this registry");
    return;
  }
  if (LookupByConstructor(isolate, constructor)) {
    exception_state->Throw(
        "NotSupportedError",
        "this constructor has already been used with this registry");
    return;
  }
  if (define_running_) {
    exception_state->Throw("NotSupportedError",
                           "this registry is already defining an element");
    return;
  }

  CustomElementDefinition* definition =
      MakeGarbageCollected<CustomElementDefinition>(isolate);
  definition->name = name;
  definition->constructor.Reset(isolate, constructor);

  // Reading the prototype and callbacks runs author code.
  define_running_ = true;
  bool ok = false;
  do {
    Local<Value> prototype_value;
    if (!constructor
             ->Get(context,
                   v8::String::NewFromUtf8Literal(isolate, "prototype"))
             .ToLocal(&prototype_value)) {
      break;
    }
    if (!prototype_value->IsObject()) {
      exception_state->Throw("TypeError",
                             "constructor.prototype is not an object");
      break;
    }
    Local<Object> prototype = prototype_value.As<Object>();
    if (!GetCallback(isolate, context, prototype, "connectedCallback",
                     &definition->connected_callback, exception_state) ||
        !GetCallback(isolate, context, prototype, "disconnectedCallback",
                     &definition->disconnected_callback, exception_state) ||
        !GetCallback(isolate, context, prototype, "adoptedCallback",
                     &definition->adopted_callback, exception_state) ||
        !GetCallback(isolate, context, prototype, "attributeChangedCallback",
                     &definition->attribute_changed_callback,
                     exception_state)) {
      break;
    }
    if (!definition->attribute_changed_callback.IsEmpty()) {
      Local<Value> observed;
      if (!constructor
               ->Get(context, v8::String::NewFromUtf8Literal(
                                  isolate, "observedAttributes"))
               .ToLocal(&observed)) {
        break;
      }
      if (observed->IsArray()) {
        Local<Array> array = observed.As<Array>();
        bool failed = false;
        for (uint32_t i = 0; i < array->Length(); ++i) {
          Local<Value> item;
          Local<v8::String> item_string;
          if (!array->Get(context, i).ToLocal(&item) ||
              !item->ToString(context).ToLocal(&item_string)) {
            failed = true;
            break;
          }
          definition->observed_attributes.push_back(
              ToAtomicString(isolate, item_string));
        }
        if (failed) break;
      }
    }
    ok = true;
  } while (false);
  define_running_ = false;
  if (!ok) return;

  definition->next = definitions_;
  definitions_ = definition;

  {
    CEReactionsScope reactions(isolate);
    UpgradeSubtree(isolate, window_->document());
  }

  auto it = when_defined_.find(name);
  if (it != when_defined_.end()) {
    Local<Promise::Resolver> resolver = it->second.Get(isolate);
    when_defined_.erase(it);
    USE(resolver->Resolve(context, constructor));
  }
}

Local<Value> CustomElementRegistry::WhenDefined(Isolate* isolate,
                                                const AtomicString& name) {
  Local<Context> context = isolate->GetCurrentContext();
  Local<Promise::Resolver> resolver;
  if (CustomElementDefinition* definition = Lookup(name)) {
    resolver = Promise::Resolver::New(context).ToLocalChecked();
    USE(resolver->Resolve(context, definition->constructor.Get(isolate)));
    return resolver->GetPromise();
  }
  auto it = when_defined_.find(name);
  if (it != when_defined_.end()) return it->second.Get(isolate)->GetPromise();
  resolver = Promise::Resolver::New(context).ToLocalChecked();
  when_defined_[name].Reset(isolate, resolver);
  return resolver->GetPromise();
}

void CustomElementRegistry::UpgradeSubtree(Isolate* isolate, Node* root) {
  std::vector<cppgc::Persistent<Element>> candidates;
  ForEachShadowIncludingDescendant(root, false, [&](Node* node, bool) {
    if (node->IsElement() && node->has_flag(Node::kIsUndefinedCustom)) {
      candidates.emplace_back(static_cast<Element*>(node));
    }
  });
  for (const auto& element : candidates) {
    CEReactionsScope::Enqueue(isolate, element.Get(),
                              CEReactionsScope::kUpgrade);
  }
}

void UpgradeElement(Isolate* isolate, Element* element,
                    CustomElementDefinition* definition) {
  if (!element->has_flag(Node::kIsUndefinedCustom)) return;
  HandleScope handle_scope(isolate);
  Window* window = element->document()->window();
  Local<Context> context = window->context(isolate);
  element->EnsureRareData(isolate)->definition = definition;
  element->set_flag(Node::kIsUndefinedCustom, false);

  // attributeChangedCallback for the attributes the element already has, and
  // connectedCallback, are queued before the constructor runs but delivered
  // after it, as the upgrade algorithm specifies.
  definition->construction_stack.emplace_back(element);
  Local<Function> constructor = definition->constructor.Get(isolate);
  TryCatch try_catch(isolate);
  Local<Object> result;
  bool ok = constructor->NewInstance(context).ToLocal(&result);
  definition->construction_stack.pop_back();
  if (!ok) {
    element->set_flag(Node::kIsUndefinedCustom, false);
    element->rare_data()->definition = nullptr;
    if (!try_catch.HasTerminated()) {
      window->ReportException(isolate, try_catch.Exception(),
                              try_catch.Message());
    }
    return;
  }
  if (!result->StrictEquals(ToV8(isolate, element))) {
    element->rare_data()->definition = nullptr;
    Local<Value> error = Exception::TypeError(v8::String::NewFromUtf8Literal(
        isolate,
        "The custom element constructor returned a different object than the "
        "element being upgraded"));
    window->ReportException(isolate, error,
                            Exception::CreateMessage(isolate, error));
    return;
  }
  element->set_flag(Node::kIsCustom, true);
  element->DidChangeState();

  CEReactionsScope reactions(isolate);
  element->SynchronizeAttributes();
  for (const Attribute& attribute : element->attributes()) {
    if (definition->Observes(attribute.name)) {
      CEReactionsScope::Enqueue(
          isolate, element, CEReactionsScope::kAttributeChanged, attribute.name,
          AtomicString(), attribute.value);
    }
  }
  if (element->is_connected()) {
    CEReactionsScope::Enqueue(isolate, element, CEReactionsScope::kConnected);
  }
}

namespace {

void RunReaction(Isolate* isolate, const PendingReaction& reaction) {
  Element* element = reaction.element.Get();
  HandleScope handle_scope(isolate);
  Window* window = element->document()->window();
  switch (reaction.kind) {
    case CEReactionsScope::kUpgrade: {
      if (!element->has_flag(Node::kIsUndefinedCustom)) return;
      CustomElementDefinition* definition =
          window->custom_elements()->Lookup(element->local_name());
      if (definition) UpgradeElement(isolate, element, definition);
      return;
    }
    case CEReactionsScope::kResourceChanged: {
      Window* view = element->document()->default_view();
      if (!view || view->resource_callback.IsEmpty()) return;
      Local<Value> args[] = {ToV8(isolate, element)};
      CallAndReport(isolate, view, view->resource_callback.Get(isolate),
                    Undefined(isolate), 1, args);
      return;
    }
    case CEReactionsScope::kConnected:
    case CEReactionsScope::kDisconnected:
    case CEReactionsScope::kAttributeChanged:
      break;
  }
  CustomElementDefinition* definition = element->definition();
  if (!definition || !element->has_flag(Node::kIsCustom)) return;
  Local<Value> receiver = ToV8(isolate, element);
  if (reaction.kind == CEReactionsScope::kConnected) {
    if (definition->connected_callback.IsEmpty()) return;
    // A queued connectedCallback is still delivered if the element has been
    // moved since; it is skipped only if the element is no longer connected.
    if (!element->is_connected()) return;
    CallAndReport(isolate, window, definition->connected_callback.Get(isolate),
                  receiver, 0, nullptr);
  } else if (reaction.kind == CEReactionsScope::kDisconnected) {
    if (definition->disconnected_callback.IsEmpty()) return;
    CallAndReport(isolate, window,
                  definition->disconnected_callback.Get(isolate), receiver, 0,
                  nullptr);
  } else {
    if (definition->attribute_changed_callback.IsEmpty()) return;
    Local<Value> args[] = {ToV8String(isolate, reaction.name),
                           ToV8StringOrNull(isolate, reaction.old_value),
                           ToV8StringOrNull(isolate, reaction.new_value),
                           Null(isolate)};
    CallAndReport(isolate, window,
                  definition->attribute_changed_callback.Get(isolate), receiver,
                  4, args);
  }
}

}  // namespace

CEReactionsScope::CEReactionsScope(Isolate* isolate) : isolate_(isolate) {
  ++IsolateData::Get()->reactions_scope_depth;
}

CEReactionsScope::~CEReactionsScope() {
  IsolateData* data = IsolateData::Get();
  if (--data->reactions_scope_depth > 0) return;
  // The outermost DOM operation is done and the tree is consistent again:
  // run what it queued. Reactions can queue more (in nested scopes, which
  // then run them themselves).
  while (!data->reactions.empty()) {
    std::vector<PendingReaction> reactions;
    reactions.swap(data->reactions);
    for (const PendingReaction& reaction : reactions) {
      if (isolate_->IsExecutionTerminating()) return;
      RunReaction(isolate_, reaction);
    }
  }
}

void CEReactionsScope::Enqueue(Isolate* isolate, Element* element, Kind kind,
                               AtomicString name, AtomicString old_value,
                               AtomicString new_value) {
  IsolateData* data = IsolateData::Get();
  PendingReaction reaction{cppgc::Persistent<Element>(element), kind,
                           std::move(name), std::move(old_value),
                           std::move(new_value)};
  if (data->reactions_scope_depth == 0) {
    // Not inside a DOM operation (e.g. the parser building a connected tree).
    RunReaction(isolate, reaction);
    return;
  }
  data->reactions.push_back(std::move(reaction));
}

}  // namespace mock_dom
}  // namespace v8
