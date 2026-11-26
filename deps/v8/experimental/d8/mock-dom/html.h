// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_HTML_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_HTML_H_

#include <string>
#include <string_view>

#include "experimental/d8/mock-dom/dom.h"

namespace v8 {
namespace mock_dom {

// Parses |html| as a fragment in the context of |context_element| (which
// decides the namespace and may be null for a plain HTML context) and appends
// the resulting nodes to |container|.
//
// This is a pragmatic HTML parser: it handles comments, doctypes, character
// references, raw-text and RCDATA elements, void elements, the common implied
// end tags, SVG/MathML foreign content and <template>. It does not implement
// the adoption agency algorithm, foster parenting or implicit <tbody>.
void ParseHTMLFragment(Isolate* isolate, ContainerNode* container,
                       Element* context_element, std::string_view html);

// Parses a complete document into the empty |document|, creating the html,
// head and body elements as the HTML parser would. Scripts are not executed;
// the page loader runs them afterwards in document order.
void ParseHTMLDocument(Isolate* isolate, Document* document,
                       std::string_view html);

// HTML fragment serialization (innerHTML / outerHTML).
void SerializeNode(const Node* node, std::string* out);
void SerializeChildren(const Node* node, std::string* out);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_HTML_H_
