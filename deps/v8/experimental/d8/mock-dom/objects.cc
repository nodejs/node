// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/objects.h"

#include <algorithm>

#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/layout.h"
#include "include/v8-container.h"
#include "include/v8-microtask-queue.h"
#include "src/base/logging.h"
#include "src/d8/d8.h"

namespace v8 {
namespace mock_dom {

void PostTask(Isolate* isolate, std::function<void()> task) {
  Shell::PostMockDOMTask(isolate, std::move(task), 0);
}

void PostDelayedTask(Isolate* isolate, std::function<void()> task,
                     double delay_in_seconds) {
  Shell::PostMockDOMTask(isolate, std::move(task), delay_in_seconds);
}

void CanvasContext::Trace(Visitor* visitor) const {
  if (DeferTraceIfConcurrent(visitor, this)) return;
  ScriptWrappable::Trace(visitor);
  visitor->Trace(canvas_);
  visitor->Trace(state);
  for (const State& saved : saved_states) visitor->Trace(saved);
}

// --- DOMTokenList ------------------------------------------------------------

void DOMTokenList::Commit(Isolate* isolate,
                          const std::vector<AtomicString>& tokens) {
  std::string joined;
  for (const AtomicString& token : tokens) {
    if (!joined.empty()) joined.push_back(' ');
    joined += token.ToUtf8();
  }
  element_->SetAttribute(isolate, AtomicString("class"),
                         AtomicString::FromUtf8(joined));
}

bool DOMTokenList::Add(Isolate* isolate,
                       const std::vector<AtomicString>& tokens) {
  std::vector<AtomicString> updated = this->tokens();
  bool changed = false;
  for (const AtomicString& token : tokens) {
    if (std::find(updated.begin(), updated.end(), token) == updated.end()) {
      updated.push_back(token);
      changed = true;
    }
  }
  if (changed) Commit(isolate, updated);
  return changed;
}

bool DOMTokenList::Remove(Isolate* isolate,
                          const std::vector<AtomicString>& tokens) {
  std::vector<AtomicString> updated = this->tokens();
  bool changed = false;
  for (const AtomicString& token : tokens) {
    auto it = std::find(updated.begin(), updated.end(), token);
    if (it != updated.end()) {
      updated.erase(it);
      changed = true;
    }
  }
  if (changed) Commit(isolate, updated);
  return changed;
}

bool DOMTokenList::Toggle(Isolate* isolate, const AtomicString& token,
                          bool has_force, bool force) {
  bool present = Contains(token);
  if (present) {
    if (has_force && force) return true;
    Remove(isolate, {token});
    return false;
  }
  if (has_force && !force) return false;
  Add(isolate, {token});
  return true;
}

bool DOMTokenList::Replace(Isolate* isolate, const AtomicString& token,
                           const AtomicString& new_token) {
  std::vector<AtomicString> updated = this->tokens();
  auto it = std::find(updated.begin(), updated.end(), token);
  if (it == updated.end()) return false;
  if (std::find(updated.begin(), updated.end(), new_token) != updated.end()) {
    updated.erase(it);
  } else {
    *it = new_token;
  }
  Commit(isolate, updated);
  return true;
}

AtomicString DOMTokenList::Value() const {
  const AtomicString& value = element_->GetAttribute(AtomicString("class"));
  return value.is_null() ? AtomicString::EmptyAtom() : value;
}

void DOMTokenList::SetValue(Isolate* isolate, const AtomicString& value) {
  element_->SetAttribute(isolate, AtomicString("class"), value);
}

// --- CSSStyleDeclaration -----------------------------------------------------

AtomicString CSSStyleDeclaration::PropertyNameFromJS(std::string_view name) {
  if (name.substr(0, 2) == "--") return AtomicString::FromUtf8(name);
  if (name == "cssFloat") return AtomicString("float");
  if (name.find('-') != std::string_view::npos) {
    return AtomicString(std::string_view(AsciiLower(name)));
  }
  std::string out;
  out.reserve(name.size() + 4);
  if (name.substr(0, 6) == "webkit" && name.size() > 6 && name[6] >= 'A' &&
      name[6] <= 'Z') {
    out.push_back('-');
  }
  for (char c : name) {
    if (c >= 'A' && c <= 'Z') {
      out.push_back('-');
      out.push_back(ToAsciiLower(c));
    } else {
      out.push_back(c);
    }
  }
  return AtomicString(std::string_view(out));
}

void CSSStyleDeclaration::Parse(std::string_view text) {
  properties_.clear();
  size_t i = 0;
  while (i < text.size()) {
    // Find the end of this declaration, skipping over quoted strings and
    // parenthesised groups such as url(data:...;base64,...).
    size_t end = i;
    int depth = 0;
    char quote = 0;
    while (end < text.size()) {
      char c = text[end];
      if (quote) {
        if (c == '\\') {
          ++end;
        } else if (c == quote) {
          quote = 0;
        }
      } else if (c == '"' || c == '\'') {
        quote = c;
      } else if (c == '(') {
        ++depth;
      } else if (c == ')') {
        if (depth > 0) --depth;
      } else if (c == ';' && depth == 0) {
        break;
      }
      ++end;
    }
    std::string_view declaration =
        text.substr(i, std::min(end, text.size()) - i);
    i = end + 1;
    size_t colon = declaration.find(':');
    if (colon == std::string_view::npos) continue;
    std::string_view name = StripHtmlSpace(declaration.substr(0, colon));
    std::string_view value = StripHtmlSpace(declaration.substr(colon + 1));
    if (name.empty() || value.empty()) continue;
    bool important = false;
    size_t bang = value.rfind('!');
    if (bang != std::string_view::npos &&
        EqualsIgnoringAsciiCase(StripHtmlSpace(value.substr(bang + 1)),
                                "important")) {
      important = true;
      value = StripHtmlSpace(value.substr(0, bang));
    }
    AtomicString property_name =
        name.substr(0, 2) == "--"
            ? AtomicString::FromUtf8(name)
            : AtomicString(std::string_view(AsciiLower(name)));
    bool found = false;
    for (Property& property : properties_) {
      if (property.name == property_name) {
        property.value = String::FromUtf8(value);
        property.important = important;
        found = true;
      }
    }
    if (!found) {
      properties_.push_back(
          {property_name, String::FromUtf8(value), important});
    }
  }
}

String CSSStyleDeclaration::GetPropertyValue(const AtomicString& name) const {
  if (computed_) return ComputedStyleValue(element_.Get(), name);
  for (const Property& property : properties_) {
    if (property.name == name) return property.value;
  }
  return String::Empty();
}

bool CSSStyleDeclaration::GetPropertyPriority(const AtomicString& name) const {
  for (const Property& property : properties_) {
    if (property.name == name) return property.important;
  }
  return false;
}

void CSSStyleDeclaration::DidMutate(Isolate* isolate) {
  attribute_is_stale_ = true;
  if (element_) {
    InvalidateStyle(element_.Get());
    element_->document()->InvalidateLayout();
  }
  if (element_ && IsolateData::Get()->observer_registration_count) {
    static const AtomicString* style = new AtomicString("style");
    QueueAttributeMutation(isolate, element_.Get(), *style, AtomicString());
  }
}

void CSSStyleDeclaration::SetProperty(Isolate* isolate,
                                      const AtomicString& name,
                                      const String& value, bool important) {
  if (computed_) return;
  if (value.empty()) {
    RemoveProperty(isolate, name);
    return;
  }
  for (Property& property : properties_) {
    if (property.name == name) {
      if (property.value == value && property.important == important) return;
      property.value = value;
      property.important = important;
      DidMutate(isolate);
      return;
    }
  }
  properties_.push_back({name, value, important});
  DidMutate(isolate);
}

String CSSStyleDeclaration::RemoveProperty(Isolate* isolate,
                                           const AtomicString& name) {
  if (computed_) return String::Empty();
  for (size_t i = 0; i < properties_.size(); ++i) {
    if (properties_[i].name != name) continue;
    String old = properties_[i].value;
    properties_.erase(properties_.begin() + i);
    DidMutate(isolate);
    return old;
  }
  return String::Empty();
}

String CSSStyleDeclaration::CssText() const {
  std::string out;
  for (const Property& property : properties_) {
    if (!out.empty()) out.push_back(' ');
    out += property.name.ToUtf8();
    out += ": ";
    out += property.value.ToUtf8();
    if (property.important) out += " !important";
    out.push_back(';');
  }
  return String::FromUtf8(out);
}

void CSSStyleDeclaration::SetCssText(Isolate* isolate, const String& text) {
  if (computed_) return;
  Parse(text.ToUtf8());
  DidMutate(isolate);
}

void CSSStyleDeclaration::SetCssTextFromAttribute(const AtomicString& value) {
  Parse(value.is_null() ? std::string() : value.ToUtf8());
  attribute_is_stale_ = false;
}

CSSStyleDeclaration* InlineStyle(Isolate* isolate, Element* element) {
  ElementRareData* rare = element->EnsureRareData(isolate);
  if (!rare->style) {
    static const AtomicString* style = new AtomicString("style");
    CSSStyleDeclaration* declaration =
        MakeGarbageCollected<CSSStyleDeclaration>(isolate, element, false);
    declaration->SetCssTextFromAttribute(element->GetAttribute(*style));
    rare->style = declaration;
  }
  return rare->style.Get();
}

// --- DOMStringMap ------------------------------------------------------------

AtomicString DOMStringMap::AttributeName(std::string_view property) {
  std::string out = "data-";
  for (char c : property) {
    if (c >= 'A' && c <= 'Z') {
      out.push_back('-');
      out.push_back(ToAsciiLower(c));
    } else {
      out.push_back(c);
    }
  }
  return AtomicString::FromUtf8(out);
}

std::string DOMStringMap::PropertyName(std::string_view attribute) {
  if (attribute.substr(0, 5) != "data-") return std::string();
  std::string out;
  for (size_t i = 5; i < attribute.size(); ++i) {
    if (attribute[i] == '-' && i + 1 < attribute.size() &&
        attribute[i + 1] >= 'a' && attribute[i + 1] <= 'z') {
      out.push_back(ToAsciiUpper(attribute[i + 1]));
      ++i;
    } else {
      out.push_back(attribute[i]);
    }
  }
  return out;
}

// --- Storage -----------------------------------------------------------------

String Storage::GetItem(const String& key) const {
  for (const auto& item : items_) {
    if (item.first == key) return item.second;
  }
  return String();
}

void Storage::SetItem(const String& key, const String& value) {
  for (auto& item : items_) {
    if (item.first == key) {
      item.second = value;
      return;
    }
  }
  items_.emplace_back(key, value);
}

void Storage::RemoveItem(const String& key) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].first == key) {
      items_.erase(items_.begin() + i);
      return;
    }
  }
}

// --- Location and History ----------------------------------------------------

namespace {

void FireHashChange(Isolate* isolate, Window* window,
                    const std::string& old_url, const std::string& new_url) {
  HandleScope handle_scope(isolate);
  Event* event =
      MakeGarbageCollected<Event>(isolate, isolate, Interface::kHashChangeEvent,
                                  AtomicString("hashchange"));
  event->is_trusted = true;
  event->set_string("oldURL", String::FromUtf8(old_url));
  event->set_string("newURL", String::FromUtf8(new_url));
  window->DispatchEvent(isolate, event);
}

}  // namespace

Location::Location(Window* window)
    : ScriptWrappable(Interface::kLocation),
      window_(window),
      url_(ParsedURL::Parse("about:blank")) {}

void Location::SetHash(Isolate* isolate, std::string_view fragment) {
  if (!fragment.empty() && fragment[0] == '#') fragment.remove_prefix(1);
  if (url_.fragment == fragment) return;
  std::string old_url = url_.Href();
  url_.fragment = std::string(fragment);
  std::string new_url = url_.Href();
  window_->history()->PushFragmentEntry(isolate);
  cppgc::Persistent<Window> window(window_.Get());
  PostTask(isolate, [isolate, window, old_url, new_url] {
    FireHashChange(isolate, window.Get(), old_url, new_url);
  });
}

void Location::Navigate(Isolate* isolate, std::string_view url) {
  ParsedURL target = ParsedURL::Resolve(url, url_);
  ParsedURL without_fragment = target;
  without_fragment.fragment.clear();
  ParsedURL current = url_;
  current.fragment.clear();
  if (target.valid && without_fragment.Href() == current.Href()) {
    SetHash(isolate, target.fragment);
    return;
  }
  std::string what = "navigation to " + std::string(url);
  window_->ReportUnsupported(isolate, what.c_str());
}

void History::Trace(Visitor* visitor) const {
  if (DeferTraceIfConcurrent(visitor, this)) return;
  ScriptWrappable::Trace(visitor);
  visitor->Trace(window_);
  for (const Entry& entry : entries_) entry.Trace(visitor);
}

Local<Value> History::State(Isolate* isolate) const {
  const Entry& entry = entries_[index_];
  if (entry.state.IsEmpty()) return Null(isolate);
  return entry.state.Get(isolate);
}

bool History::PushOrReplace(Isolate* isolate, bool push, Local<Value> state,
                            bool has_url, std::string_view url,
                            ExceptionState* exception_state) {
  Location* location = window_->location();
  ParsedURL target = location->url();
  if (has_url) {
    target = ParsedURL::Resolve(url, location->url());
    if (!target.valid || target.Origin() != location->url().Origin()) {
      return exception_state->Throw(
          "SecurityError",
          "A history state object with URL '" + std::string(url) +
              "' cannot be created in a document with origin '" +
              location->url().Origin() + "'.");
    }
  }
  entries_[index_].href = location->url().Href();
  if (push) {
    entries_.resize(index_ + 1);
    entries_.emplace_back();
    ++index_;
  }
  entries_[index_].href = target.Href();
  entries_[index_].state.Reset(isolate, state);
  location->SetURLWithoutNavigation(target);
  return true;
}

void History::PushFragmentEntry(Isolate* isolate) {
  entries_.resize(index_ + 1);
  entries_.emplace_back();
  ++index_;
  entries_[index_].href = window_->location()->url().Href();
}

void History::Go(Isolate* isolate, int delta) {
  if (delta == 0) return;
  cppgc::Persistent<History> self(this);
  PostTask(isolate, [isolate, self, delta] {
    History* history = self.Get();
    int64_t target = static_cast<int64_t>(history->index_) + delta;
    if (target < 0 ||
        target >= static_cast<int64_t>(history->entries_.size())) {
      return;
    }
    HandleScope handle_scope(isolate);
    Window* window = history->window_.Get();
    Location* location = window->location();
    ParsedURL old_url = location->url();
    history->entries_[history->index_].href = old_url.Href();
    history->index_ = static_cast<size_t>(target);
    ParsedURL new_url = ParsedURL::Parse(history->entries_[target].href);
    if (new_url.valid) location->SetURLWithoutNavigation(new_url);

    Event* event = MakeGarbageCollected<Event>(
        isolate, isolate, Interface::kPopStateEvent, AtomicString("popstate"));
    event->is_trusted = true;
    event->any_slots[event->FieldIndex("state")].Reset(isolate,
                                                       history->State(isolate));
    window->DispatchEvent(isolate, event);
    if (new_url.valid && new_url.fragment != old_url.fragment) {
      FireHashChange(isolate, window, old_url.Href(), new_url.Href());
    }
  });
}

// --- AbortSignal -------------------------------------------------------------

void AbortSignal::Trace(Visitor* visitor) const {
  if (DeferTraceIfConcurrent(visitor, this)) return;
  EventTarget::Trace(visitor);
  visitor->Trace(reason_);
  for (const Removal& removal : removals_) removal.Trace(visitor);
}

Local<Value> AbortSignal::Reason(Isolate* isolate) const {
  if (reason_.IsEmpty()) return Undefined(isolate);
  return reason_.Get(isolate);
}

void AbortSignal::Abort(Isolate* isolate, Local<Value> reason) {
  if (aborted_) return;
  aborted_ = true;
  reason_.Reset(isolate, reason);
  std::vector<Removal> removals;
  removals.swap(removals_);
  for (const Removal& removal : removals) {
    removal.target->RemoveEventListener(
        isolate, removal.type, removal.callback.Get(isolate), removal.capture);
  }
  Event* event = MakeGarbageCollected<Event>(
      isolate, isolate, Interface::kEvent, AtomicString("abort"));
  event->is_trusted = true;
  DispatchEvent(isolate, event);
}

void AbortSignal::AddListenerRemoval(Isolate* isolate, EventTarget* target,
                                     AtomicString type, Local<Value> callback,
                                     bool capture) {
  Removal removal;
  removal.target = target;
  removal.type = std::move(type);
  removal.callback.Reset(isolate, callback);
  removal.capture = capture;
  removals_.push_back(std::move(removal));
}

// --- MutationObserver --------------------------------------------------------

void MutationRecord::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(target);
  visitor->Trace(added_nodes);
  visitor->Trace(removed_nodes);
  visitor->Trace(previous_sibling);
  visitor->Trace(next_sibling);
}

void MutationObserver::Trace(Visitor* visitor) const {
  if (DeferTraceIfConcurrent(visitor, this)) return;
  ScriptWrappable::Trace(visitor);
  visitor->Trace(callback_);
  for (const auto& record : records_) visitor->Trace(record);
  for (const auto& target : targets_) visitor->Trace(target);
}

void MutationObserver::Observe(Isolate* isolate, Node* target,
                               const MutationObserverOptions& options) {
  for (MutationObserverRegistration* r = target->observer_registrations(); r;
       r = r->next) {
    if (r->observer == this) {
      r->options = options;
      return;
    }
  }
  MutationObserverRegistration* registration =
      MakeGarbageCollected<MutationObserverRegistration>(isolate);
  registration->observer = this;
  registration->options = options;
  registration->next = target->observer_registrations();
  target->set_observer_registrations(registration);
  targets_.emplace_back(target);
  ++IsolateData::Get()->observer_registration_count;
}

void MutationObserver::Disconnect() {
  for (const auto& target : targets_) {
    MutationObserverRegistration* prev = nullptr;
    for (MutationObserverRegistration* r = target->observer_registrations(); r;
         prev = r, r = r->next) {
      if (r->observer != this) continue;
      if (prev) {
        prev->next = r->next;
      } else {
        target->set_observer_registrations(r->next);
      }
      --IsolateData::Get()->observer_registration_count;
      break;
    }
  }
  targets_.clear();
  records_.clear();
}

Local<Array> MutationObserver::TakeRecords(Isolate* isolate) {
  // The records stay in records_, where the GC sees them, until each one has
  // a wrapper.
  LocalVector<Value> wrappers(isolate);
  wrappers.reserve(records_.size());
  for (const auto& record : records_) {
    wrappers.push_back(ToV8(isolate, record.Get()));
  }
  records_.clear();
  return Array::New(isolate, wrappers.data(), wrappers.size());
}

namespace {

void DeliverMutationRecords(const FunctionCallbackInfo<Value>& info) {
  IsolateData* isolate_data = IsolateData::Get();
  isolate_data->observer_microtask_queued = false;
  std::vector<cppgc::Persistent<MutationObserver>> observers;
  observers.swap(isolate_data->active_observers);
  for (const auto& observer : observers) {
    observer->Deliver(isolate_data->isolate);
  }
}

}  // namespace

void MutationObserver::Enqueue(Isolate* isolate, MutationRecord* record) {
  IsolateData* data = IsolateData::Get();
  if (records_.empty()) data->active_observers.emplace_back(this);
  records_.emplace_back(record);
  if (!data->observer_microtask_queued) {
    Local<Context> context;
    Local<Function> deliver;
    if (callback_.Get(isolate)->GetCreationContext(isolate).ToLocal(&context) &&
        Function::New(context, DeliverMutationRecords).ToLocal(&deliver)) {
      data->observer_microtask_queued = true;
      context->GetMicrotaskQueue()->EnqueueMicrotask(isolate, deliver);
    }
  }
}

void MutationObserver::Deliver(Isolate* isolate) {
  if (records_.empty()) return;
  HandleScope handle_scope(isolate);
  Local<Function> callback = callback_.Get(isolate);
  Local<Context> context;
  if (!callback->GetCreationContext(isolate).ToLocal(&context)) return;
  Context::Scope context_scope(context);
  Local<Value> self = ToV8(isolate, this);
  Local<Value> args[] = {TakeRecords(isolate), self};
  CallAndReport(isolate, nullptr, callback, self, 2, args);
}

namespace {

enum class MutationKind { kChildList, kAttributes, kCharacterData };

struct InterestedObserver {
  CPPGC_STACK_ALLOCATED();

 public:
  // Kept alive by its registration on the observed node.
  MutationObserver* observer;
  bool wants_old_value;
};

void CollectObservers(Node* target, MutationKind kind,
                      const AtomicString& attribute_name,
                      std::vector<InterestedObserver>* out) {
  for (Node* node = target; node; node = node->parent()) {
    for (MutationObserverRegistration* r = node->observer_registrations(); r;
         r = r->next) {
      const MutationObserverOptions& options = r->options;
      if (node != target && !options.subtree) continue;
      bool wants_old_value = false;
      switch (kind) {
        case MutationKind::kChildList:
          if (!options.child_list) continue;
          break;
        case MutationKind::kAttributes:
          if (!options.attributes) continue;
          if (options.has_attribute_filter &&
              std::find(options.attribute_filter.begin(),
                        options.attribute_filter.end(),
                        attribute_name) == options.attribute_filter.end()) {
            continue;
          }
          wants_old_value = options.attribute_old_value;
          break;
        case MutationKind::kCharacterData:
          if (!options.character_data) continue;
          wants_old_value = options.character_data_old_value;
          break;
      }
      bool found = false;
      for (InterestedObserver& existing : *out) {
        if (existing.observer == r->observer.Get()) {
          existing.wants_old_value |= wants_old_value;
          found = true;
        }
      }
      if (!found) out->push_back({r->observer.Get(), wants_old_value});
    }
  }
}

}  // namespace

void QueueChildListMutation(Isolate* isolate, Node* target, Node* added,
                            Node* removed, Node* previous_sibling,
                            Node* next_sibling) {
  MOCK_DOM_LOCAL_NODES
  std::vector<InterestedObserver> observers;
  CollectObservers(target, MutationKind::kChildList, AtomicString(),
                   &observers);
  if (observers.empty()) return;
  MOCK_DOM_LOCAL_NODES
  std::vector<Node*> added_nodes;
  MOCK_DOM_LOCAL_NODES
  std::vector<Node*> removed_nodes;
  if (added) added_nodes.push_back(added);
  if (removed) removed_nodes.push_back(removed);
  for (const InterestedObserver& interested : observers) {
    MutationRecord* record = MakeGarbageCollected<MutationRecord>(isolate);
    record->type = AtomicString("childList");
    record->target = target;
    record->added_nodes = NodeList::CreateStatic(isolate, added_nodes);
    record->removed_nodes = NodeList::CreateStatic(isolate, removed_nodes);
    record->previous_sibling = previous_sibling;
    record->next_sibling = next_sibling;
    interested.observer->Enqueue(isolate, record);
  }
}

void QueueAttributeMutation(Isolate* isolate, Element* target,
                            const AtomicString& name,
                            const AtomicString& old_value) {
  MOCK_DOM_LOCAL_NODES
  std::vector<InterestedObserver> observers;
  CollectObservers(target, MutationKind::kAttributes, name, &observers);
  for (const InterestedObserver& interested : observers) {
    MutationRecord* record = MakeGarbageCollected<MutationRecord>(isolate);
    record->type = AtomicString("attributes");
    record->target = target;
    record->attribute_name = name;
    if (interested.wants_old_value) record->old_value = old_value;
    MOCK_DOM_LOCAL_NODES
    std::vector<Node*> none;
    record->added_nodes = NodeList::CreateStatic(isolate, none);
    record->removed_nodes = NodeList::CreateStatic(isolate, none);
    interested.observer->Enqueue(isolate, record);
  }
}

void QueueCharacterDataMutation(Isolate* isolate, Node* target,
                                const String& old_value) {
  MOCK_DOM_LOCAL_NODES
  std::vector<InterestedObserver> observers;
  CollectObservers(target, MutationKind::kCharacterData, AtomicString(),
                   &observers);
  for (const InterestedObserver& interested : observers) {
    MutationRecord* record = MakeGarbageCollected<MutationRecord>(isolate);
    record->type = AtomicString("characterData");
    record->target = target;
    if (interested.wants_old_value) record->old_value = old_value;
    MOCK_DOM_LOCAL_NODES
    std::vector<Node*> none;
    record->added_nodes = NodeList::CreateStatic(isolate, none);
    record->removed_nodes = NodeList::CreateStatic(isolate, none);
    interested.observer->Enqueue(isolate, record);
  }
}

}  // namespace mock_dom
}  // namespace v8
