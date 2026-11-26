// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The smaller DOM objects that hang off nodes and windows.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_OBJECTS_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_OBJECTS_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "experimental/d8/mock-dom/dom.h"
#include "experimental/d8/mock-dom/url.h"

namespace v8 {
namespace mock_dom {

// Posts |task| to the isolate's foreground task queue (a macrotask).
void PostTask(Isolate* isolate, std::function<void()> task);
// Like PostTask, but the task runs no earlier than |delay_in_seconds| from
// now. Delayed tasks run in the order of their deadlines.
void PostDelayedTask(Isolate* isolate, std::function<void()> task,
                     double delay_in_seconds);

// --- element.classList -------------------------------------------------------

class DOMTokenList final : public ScriptWrappable {
 public:
  explicit DOMTokenList(Element* element)
      : ScriptWrappable(Interface::kDOMTokenList), element_(element) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(element_);
  }
  Window* WrapperWindow() const override { return element_->WrapperWindow(); }

  Element* element() const { return element_.Get(); }
  const std::vector<AtomicString>& tokens() const {
    return element_->class_names();
  }
  bool Contains(const AtomicString& token) const {
    return element_->HasClass(token);
  }
  // Each returns whether the token set changed.
  bool Add(Isolate* isolate, const std::vector<AtomicString>& tokens);
  bool Remove(Isolate* isolate, const std::vector<AtomicString>& tokens);
  bool Toggle(Isolate* isolate, const AtomicString& token, bool has_force,
              bool force);
  bool Replace(Isolate* isolate, const AtomicString& token,
               const AtomicString& new_token);
  AtomicString Value() const;
  void SetValue(Isolate* isolate, const AtomicString& value);

 private:
  void Commit(Isolate* isolate, const std::vector<AtomicString>& tokens);
  Member<Element> element_;
};

// --- element.style / getComputedStyle ---------------------------------------

class CSSStyleDeclaration final : public ScriptWrappable {
 public:
  struct Property {
    AtomicString name;  // lower-case, dashed
    String value;
    bool important = false;
  };

  CSSStyleDeclaration(Element* element, bool computed)
      : ScriptWrappable(Interface::kCSSStyleDeclaration),
        element_(element),
        computed_(computed) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(element_);
  }
  Window* WrapperWindow() const override {
    return element_ ? element_->WrapperWindow() : nullptr;
  }

  Element* element() const { return element_.Get(); }
  bool computed() const { return computed_; }
  const std::vector<Property>& properties() const { return properties_; }

  String GetPropertyValue(const AtomicString& name) const;
  bool GetPropertyPriority(const AtomicString& name) const;
  void SetProperty(Isolate* isolate, const AtomicString& name,
                   const String& value, bool important);
  String RemoveProperty(Isolate* isolate, const AtomicString& name);
  String CssText() const;
  void SetCssText(Isolate* isolate, const String& text);
  // Re-parses after the content attribute was written directly.
  void SetCssTextFromAttribute(const AtomicString& value);

  bool attribute_is_stale() const { return attribute_is_stale_; }
  void clear_attribute_is_stale() { attribute_is_stale_ = false; }

  // Maps a JS property name (`backgroundColor`, `cssFloat`, `--x`,
  // `background-color`) to a CSS property name.
  static AtomicString PropertyNameFromJS(std::string_view name);

 private:
  void Parse(std::string_view text);
  void DidMutate(Isolate* isolate);

  Member<Element> element_;
  bool computed_;
  bool attribute_is_stale_ = false;
  std::vector<Property> properties_;
};

// Returns the inline style object of |element|, creating it on first use.
CSSStyleDeclaration* InlineStyle(Isolate* isolate, Element* element);

// --- element.dataset, element.attributes ------------------------------------

class DOMStringMap final : public ScriptWrappable {
 public:
  explicit DOMStringMap(Element* element)
      : ScriptWrappable(Interface::kDOMStringMap), element_(element) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(element_);
  }
  Window* WrapperWindow() const override { return element_->WrapperWindow(); }
  Element* element() const { return element_.Get(); }
  // "fooBar" -> "data-foo-bar".
  static AtomicString AttributeName(std::string_view property);
  // "data-foo-bar" -> "fooBar"; empty if not a data attribute.
  static std::string PropertyName(std::string_view attribute);

 private:
  Member<Element> element_;
};

class NamedNodeMap final : public ScriptWrappable {
 public:
  explicit NamedNodeMap(Element* element)
      : ScriptWrappable(Interface::kNamedNodeMap), element_(element) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(element_);
  }
  Window* WrapperWindow() const override { return element_->WrapperWindow(); }
  Element* element() const { return element_.Get(); }

 private:
  Member<Element> element_;
};

class Attr final : public ScriptWrappable {
 public:
  Attr(Element* element, AtomicString name)
      : ScriptWrappable(Interface::kAttr),
        element_(element),
        name_(std::move(name)) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(element_);
  }
  Window* WrapperWindow() const override { return element_->WrapperWindow(); }
  Element* element() const { return element_.Get(); }
  const AtomicString& name() const { return name_; }

 private:
  Member<Element> element_;
  AtomicString name_;
};

// The 2D context of a <canvas>. There is no rasterizer: drawing calls are
// accepted and counted, state is tracked, and nothing is painted.
class CanvasContext final : public ScriptWrappable {
 public:
  explicit CanvasContext(Element* canvas)
      : ScriptWrappable(Interface::kCanvasRenderingContext2D),
        canvas_(canvas) {}
  void Trace(Visitor* visitor) const override;
  Window* WrapperWindow() const override { return canvas_->WrapperWindow(); }
  Element* canvas() const { return canvas_.Get(); }

  // Drawing state that pages read back. Styles may be strings, gradients or
  // patterns, so they are kept as JS values.
  struct State {
    CPPGC_DISALLOW_NEW();

   public:
    void Trace(Visitor* visitor) const {
      visitor->Trace(fill_style);
      visitor->Trace(stroke_style);
    }
    TracedReference<Value> fill_style;
    TracedReference<Value> stroke_style;
    std::string font = "10px sans-serif";
    std::string text_align = "start";
    std::string text_baseline = "alphabetic";
    std::string line_cap = "butt";
    std::string line_join = "miter";
    std::string global_composite_operation = "source-over";
    double line_width = 1;
    double miter_limit = 10;
    double global_alpha = 1;
    double line_dash_offset = 0;
    double shadow_blur = 0;
    double shadow_offset_x = 0;
    double shadow_offset_y = 0;
    std::string shadow_color = "rgba(0, 0, 0, 0)";
    std::vector<double> line_dash;
    // Current transform [a b c d e f].
    double transform[6] = {1, 0, 0, 1, 0, 0};
  };
  State state;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<State> saved_states;
  // Number of drawing operations issued, for harness post-conditions.
  uint64_t draw_calls = 0;

 private:
  Member<Element> canvas_;
};

class DOMRect final : public ScriptWrappable {
 public:
  DOMRect(double x, double y, double width, double height)
      : ScriptWrappable(Interface::kDOMRect),
        x(x),
        y(y),
        width(width),
        height(height) {}
  double x, y, width, height;
};

// --- Storage, Location, History ---------------------------------------------

class Storage final : public ScriptWrappable {
 public:
  explicit Storage(Window* window)
      : ScriptWrappable(Interface::kStorage), window_(window) {}
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(window_);
  }
  Window* WrapperWindow() const override { return window_.Get(); }

  uint32_t length() const { return static_cast<uint32_t>(items_.size()); }
  String Key(uint32_t index) const {
    return index < items_.size() ? items_[index].first : String();
  }
  String GetItem(const String& key) const;
  void SetItem(const String& key, const String& value);
  void RemoveItem(const String& key);
  void Clear() { items_.clear(); }

 private:
  Member<Window> window_;
  std::vector<std::pair<String, String>> items_;
};

class Location final : public ScriptWrappable {
 public:
  explicit Location(Window* window);
  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(window_);
  }
  Window* WrapperWindow() const override { return window_.Get(); }

  const ParsedURL& url() const { return url_; }
  // Used by the loader when a document is loaded, and by history.
  void SetURLWithoutNavigation(const ParsedURL& url) { url_ = url; }
  // Fragment navigation: updates the URL, adds a history entry and queues
  // `hashchange`.
  void SetHash(Isolate* isolate, std::string_view fragment);
  // location.href = / assign() / anchor activation. Only navigations within
  // the document are supported; anything else is reported loudly.
  void Navigate(Isolate* isolate, std::string_view url);
  void FollowHyperlink(Isolate* isolate, const AtomicString& href) {
    Navigate(isolate, href.ToUtf8());
  }

 private:
  Member<Window> window_;
  ParsedURL url_;
};

class History final : public ScriptWrappable {
 public:
  explicit History(Window* window)
      : ScriptWrappable(Interface::kHistory), window_(window) {
    entries_.emplace_back();
  }
  void Trace(Visitor* visitor) const override;
  Window* WrapperWindow() const override { return window_.Get(); }

  uint32_t length() const { return static_cast<uint32_t>(entries_.size()); }
  Local<Value> State(Isolate* isolate) const;
  // pushState / replaceState. The state is kept by reference rather than
  // structured-cloned.
  bool PushOrReplace(Isolate* isolate, bool push, Local<Value> state,
                     bool has_url, std::string_view url,
                     ExceptionState* exception_state);
  // Records a fragment navigation as a new entry.
  void PushFragmentEntry(Isolate* isolate);
  // back / forward / go: asynchronous, fires `popstate` (and `hashchange`).
  void Go(Isolate* isolate, int delta);

 private:
  struct Entry {
    CPPGC_DISALLOW_NEW();

   public:
    void Trace(Visitor* visitor) const { visitor->Trace(state); }
    std::string href;
    TracedReference<Value> state;
  };
  Member<Window> window_;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<Entry> entries_;
  size_t index_ = 0;
};

// --- MutationObserver --------------------------------------------------------

class MutationRecord final : public ScriptWrappable {
 public:
  MutationRecord() : ScriptWrappable(Interface::kMutationRecord) {}
  void Trace(Visitor* visitor) const override;
  Window* WrapperWindow() const override { return target->WrapperWindow(); }

  AtomicString type;
  Member<Node> target;
  Member<NodeList> added_nodes;
  Member<NodeList> removed_nodes;
  Member<Node> previous_sibling;
  Member<Node> next_sibling;
  AtomicString attribute_name;
  String old_value;
};

struct MutationObserverOptions {
  bool child_list = false;
  bool attributes = false;
  bool character_data = false;
  bool subtree = false;
  bool attribute_old_value = false;
  bool character_data_old_value = false;
  bool has_attribute_filter = false;
  std::vector<AtomicString> attribute_filter;
};

class MutationObserver final : public ScriptWrappable {
 public:
  MutationObserver(Isolate* isolate, Local<Function> callback)
      : ScriptWrappable(Interface::kMutationObserver) {
    callback_.Reset(isolate, callback);
  }
  void Trace(Visitor* visitor) const override;

  void Observe(Isolate* isolate, Node* target,
               const MutationObserverOptions& options);
  void Disconnect();
  // Returns the queued records as a JS array and clears the queue.
  Local<Array> TakeRecords(Isolate* isolate);
  void Enqueue(Isolate* isolate, MutationRecord* record);
  void Deliver(Isolate* isolate);

 private:
  TracedReference<Function> callback_;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<Member<MutationRecord>> records_;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<Member<Node>> targets_;
};

// One observer watching one node. Stored as a linked list on the node.
class MutationObserverRegistration final
    : public cppgc::GarbageCollected<MutationObserverRegistration> {
 public:
  void Trace(Visitor* visitor) const {
    visitor->Trace(observer);
    visitor->Trace(next);
  }
  Member<MutationObserver> observer;
  MutationObserverOptions options;
  Member<MutationObserverRegistration> next;
};

// --- AbortController / AbortSignal -------------------------------------------

class AbortSignal final : public EventTarget {
 public:
  AbortSignal() : EventTarget(Interface::kAbortSignal) {}
  void Trace(Visitor* visitor) const override;

  bool aborted() const { return aborted_; }
  Local<Value> Reason(Isolate* isolate) const;
  void Abort(Isolate* isolate, Local<Value> reason);
  // addEventListener({signal}): removes the listener when the signal aborts.
  void AddListenerRemoval(Isolate* isolate, EventTarget* target,
                          AtomicString type, Local<Value> callback,
                          bool capture);

 private:
  struct Removal {
    CPPGC_DISALLOW_NEW();

   public:
    void Trace(Visitor* visitor) const {
      visitor->Trace(target);
      visitor->Trace(callback);
    }
    Member<EventTarget> target;
    AtomicString type;
    TracedReference<Value> callback;
    bool capture;
  };
  bool aborted_ = false;
  TracedReference<Value> reason_;
  MOCK_DOM_TRACED_CONTAINER
  std::vector<Removal> removals_;
};

void QueueChildListMutation(Isolate* isolate, Node* target, Node* added,
                            Node* removed, Node* previous_sibling,
                            Node* next_sibling);
void QueueAttributeMutation(Isolate* isolate, Element* target,
                            const AtomicString& name,
                            const AtomicString& old_value);
void QueueCharacterDataMutation(Isolate* isolate, Node* target,
                                const String& old_value);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_OBJECTS_H_
