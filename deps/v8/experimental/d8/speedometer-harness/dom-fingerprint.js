// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A structural summary of a document's body, including shadow trees. The
// same function runs in a real browser and in d8 --experimental-mock-dom;
// comparing the two after each benchmark step shows whether a workload built
// the same DOM in both.
function domFingerprint(doc) {
  const result = {elements: 0, textNodes: 0, textLength: 0, attributes: 0,
                  tags: {}};
  function walk(node) {
    for (let child = node.firstChild; child; child = child.nextSibling) {
      if (child.nodeType === 1) {
        result.elements++;
        result.attributes += child.attributes.length;
        const tag = child.localName;
        result.tags[tag] = (result.tags[tag] || 0) + 1;
        if (child.shadowRoot) walk(child.shadowRoot);
        walk(child);
      } else if (child.nodeType === 3) {
        // Whitespace-only text depends on parser details, not on the app.
        const text = child.nodeValue.trim();
        if (text.length > 0) {
          result.textNodes++;
          result.textLength += text.length;
        }
      }
    }
  }
  if (doc.body) walk(doc.body);
  const sorted = {};
  for (const tag of Object.keys(result.tags).sort()) {
    sorted[tag] = result.tags[tag];
  }
  result.tags = sorted;
  return result;
}
