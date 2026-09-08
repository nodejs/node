// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --expose-externalize-string
// Flags: --gc-global

// String.prototype.indexOf takes a slice of the receiver before flattening the
// search string. A slice into an external string is a raw pointer that doesn't
// keep the string alive, so the allocation done while flattening the search
// string could collect the receiver and free its resource.
//
// External strings are only disposed by major GCs, so the flattening allocation
// has to hit one. --gc-global just makes that deterministic; the bug also
// reproduces under ordinary old space pressure.

let subject = null;

function makeExternal(s) {
  const str = createExternalizableString(s);
  externalizeString(str, true);
  return str;
}

function indexOfAndDropSubject(search) {
  const s = subject;
  subject = null;
  // Make the allocation that flattens {search} trigger a GC.
  %SimulateNewspaceFull();
  return s.indexOf(search);
}

%PrepareFunctionForOptimization(indexOfAndDropSubject);
for (let i = 0; i < 3; i++) {
  subject = makeExternal('a'.repeat(64));
  indexOfAndDropSubject('b' + 'c');
}
%OptimizeFunctionOnNextCall(indexOfAndDropSubject);

subject = makeExternal('A'.repeat(20 * 1024));
// A cons string, so that indexOf has to flatten it.
const search = 'B'.repeat(1024) + 'C'.repeat(1024);
assertEquals(-1, indexOfAndDropSubject(search));
