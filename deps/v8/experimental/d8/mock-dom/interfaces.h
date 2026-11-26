// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_INTERFACES_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_INTERFACES_H_

#include <cstdint>

namespace v8 {
namespace mock_dom {

// Every Web IDL interface the mock DOM exposes, as V(Name, Parent). Each one
// gets its own FunctionTemplate, so wrappers of different interfaces have
// different hidden classes and prototype chains, as they do in a browser.
// Parents must be listed before their children.
#define MOCK_DOM_INTERFACE_LIST(V)               \
  V(EventTarget, None)                           \
  V(Window, EventTarget)                         \
  V(Node, EventTarget)                           \
  V(Document, Node)                              \
  V(DocumentFragment, Node)                      \
  V(ShadowRoot, DocumentFragment)                \
  V(DocumentType, Node)                          \
  V(CharacterData, Node)                         \
  V(Text, CharacterData)                         \
  V(Comment, CharacterData)                      \
  V(Element, Node)                               \
  /* HTML elements */                            \
  V(HTMLElement, Element)                        \
  V(HTMLUnknownElement, HTMLElement)             \
  V(HTMLHtmlElement, HTMLElement)                \
  V(HTMLHeadElement, HTMLElement)                \
  V(HTMLBodyElement, HTMLElement)                \
  V(HTMLTitleElement, HTMLElement)               \
  V(HTMLMetaElement, HTMLElement)                \
  V(HTMLLinkElement, HTMLElement)                \
  V(HTMLStyleElement, HTMLElement)               \
  V(HTMLScriptElement, HTMLElement)              \
  V(HTMLDivElement, HTMLElement)                 \
  V(HTMLSpanElement, HTMLElement)                \
  V(HTMLParagraphElement, HTMLElement)           \
  V(HTMLHeadingElement, HTMLElement)             \
  V(HTMLPreElement, HTMLElement)                 \
  V(HTMLBRElement, HTMLElement)                  \
  V(HTMLHRElement, HTMLElement)                  \
  V(HTMLAnchorElement, HTMLElement)              \
  V(HTMLImageElement, HTMLElement)               \
  V(HTMLUListElement, HTMLElement)               \
  V(HTMLOListElement, HTMLElement)               \
  V(HTMLLIElement, HTMLElement)                  \
  V(HTMLDListElement, HTMLElement)               \
  V(HTMLInputElement, HTMLElement)               \
  V(HTMLButtonElement, HTMLElement)              \
  V(HTMLLabelElement, HTMLElement)               \
  V(HTMLFormElement, HTMLElement)                \
  V(HTMLFieldSetElement, HTMLElement)            \
  V(HTMLSelectElement, HTMLElement)              \
  V(HTMLOptionElement, HTMLElement)              \
  V(HTMLTextAreaElement, HTMLElement)            \
  V(HTMLTableElement, HTMLElement)               \
  V(HTMLTableSectionElement, HTMLElement)        \
  V(HTMLTableRowElement, HTMLElement)            \
  V(HTMLTableCellElement, HTMLElement)           \
  V(HTMLCanvasElement, HTMLElement)              \
  V(HTMLTemplateElement, HTMLElement)            \
  V(HTMLSlotElement, HTMLElement)                \
  V(HTMLIFrameElement, HTMLElement)              \
  V(HTMLTimeElement, HTMLElement)                \
  V(HTMLPictureElement, HTMLElement)             \
  V(HTMLSourceElement, HTMLElement)              \
  V(HTMLDetailsElement, HTMLElement)             \
  V(HTMLDialogElement, HTMLElement)              \
  /* SVG elements */                             \
  V(SVGElement, Element)                         \
  V(SVGGraphicsElement, SVGElement)              \
  V(SVGSVGElement, SVGGraphicsElement)           \
  V(SVGGElement, SVGGraphicsElement)             \
  V(SVGDefsElement, SVGGraphicsElement)          \
  V(SVGUseElement, SVGGraphicsElement)           \
  V(SVGImageElement, SVGGraphicsElement)         \
  V(SVGForeignObjectElement, SVGGraphicsElement) \
  V(SVGGeometryElement, SVGGraphicsElement)      \
  V(SVGPathElement, SVGGeometryElement)          \
  V(SVGRectElement, SVGGeometryElement)          \
  V(SVGCircleElement, SVGGeometryElement)        \
  V(SVGEllipseElement, SVGGeometryElement)       \
  V(SVGLineElement, SVGGeometryElement)          \
  V(SVGPolylineElement, SVGGeometryElement)      \
  V(SVGPolygonElement, SVGGeometryElement)       \
  V(SVGTextContentElement, SVGGraphicsElement)   \
  V(SVGTextElement, SVGTextContentElement)       \
  V(SVGTSpanElement, SVGTextContentElement)      \
  V(SVGClipPathElement, SVGElement)              \
  V(SVGLinearGradientElement, SVGElement)        \
  V(SVGStopElement, SVGElement)                  \
  V(SVGTitleElement, SVGElement)                 \
  /* Events */                                   \
  V(Event, None)                                 \
  V(UIEvent, Event)                              \
  V(MouseEvent, UIEvent)                         \
  V(PointerEvent, MouseEvent)                    \
  V(WheelEvent, MouseEvent)                      \
  V(DragEvent, MouseEvent)                       \
  V(KeyboardEvent, UIEvent)                      \
  V(InputEvent, UIEvent)                         \
  V(FocusEvent, UIEvent)                         \
  V(CompositionEvent, UIEvent)                   \
  V(CustomEvent, Event)                          \
  V(ErrorEvent, Event)                           \
  V(HashChangeEvent, Event)                      \
  V(PopStateEvent, Event)                        \
  V(MessageEvent, Event)                         \
  V(PromiseRejectionEvent, Event)                \
  V(SubmitEvent, Event)                          \
  V(ProgressEvent, Event)                        \
  V(TransitionEvent, Event)                      \
  V(AnimationEvent, Event)                       \
  V(ClipboardEvent, Event)                       \
  /* Collections and helper objects */           \
  V(NodeList, None)                              \
  V(HTMLCollection, None)                        \
  V(HTMLAllCollection, None)                     \
  V(DOMTokenList, None)                          \
  V(CSSStyleDeclaration, None)                   \
  V(DOMStringMap, None)                          \
  V(NamedNodeMap, None)                          \
  V(Attr, None)                                  \
  V(DOMException, None)                          \
  V(DOMRect, None)                               \
  V(DOMRectList, None)                           \
  V(DOMStringList, None)                         \
  V(IDBFactory, None)                            \
  V(IDBRequest, EventTarget)                     \
  V(IDBOpenDBRequest, IDBRequest)                \
  V(IDBDatabase, EventTarget)                    \
  V(IDBTransaction, EventTarget)                 \
  V(IDBObjectStore, None)                        \
  V(IDBIndex, None)                              \
  V(IDBCursor, None)                             \
  V(IDBCursorWithValue, IDBCursor)               \
  V(IDBKeyRange, None)                           \
  V(IDBVersionChangeEvent, Event)                \
  V(Storage, None)                               \
  V(Location, None)                              \
  V(History, None)                               \
  V(CustomElementRegistry, None)                 \
  V(MutationObserver, None)                      \
  V(MutationRecord, None)                        \
  V(Range, None)                                 \
  V(Selection, None)                             \
  V(TreeWalker, None)                            \
  V(NodeIterator, None)                          \
  V(DOMParser, None)                             \
  V(XMLSerializer, None)                         \
  V(DOMImplementation, None)                     \
  V(CSSStyleSheet, None)                         \
  V(MediaQueryList, EventTarget)                 \
  V(ResizeObserver, None)                        \
  V(IntersectionObserver, None)                  \
  V(AbortSignal, EventTarget)                    \
  V(AbortController, None)                       \
  V(MessagePort, EventTarget)                    \
  V(MessageChannel, None)                        \
  V(URL, None)                                   \
  V(URLSearchParams, None)                       \
  V(CanvasRenderingContext2D, None)              \
  V(CanvasGradient, None)                        \
  V(CanvasPattern, None)                         \
  V(TextMetrics, None)                           \
  V(Path2D, None)                                \
  V(ImageData, None)

enum class Interface : uint16_t {
#define V(Name, Parent) k##Name,
  MOCK_DOM_INTERFACE_LIST(V)
#undef V
      kCount,
  kNone = kCount,
};

constexpr int kInterfaceCount = static_cast<int>(Interface::kCount);

const char* InterfaceName(Interface interface);
Interface InterfaceParent(Interface interface);
bool InterfaceInheritsFrom(Interface interface, Interface ancestor);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_INTERFACES_H_
