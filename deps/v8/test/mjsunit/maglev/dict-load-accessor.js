// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan

// A dictionary-mode load finds its entry by the cached index and checks the
// kind of the property there: an accessor in place of the data property, and
// an entry past the end of a dictionary that shrank, take the fallback.

function load(o) {
  return o.target;
}

function makeDictionary() {
  const o = {};
  // Enough properties to put the object in dictionary mode.
  for (let i = 0; i < 150; i++) o["p" + i] = i;
  o.target = "data";
  return o;
}

const dict = makeDictionary();
%PrepareFunctionForOptimization(load);
assertEquals("data", load(dict));
assertEquals("data", load(dict));
%OptimizeMaglevOnNextCall(load);
assertEquals("data", load(dict));
assertMaglevved(load);

// The same entry now holds an accessor.
Object.defineProperty(dict, "target",
                      {get() { return "accessor"; }, configurable: true});
assertEquals("accessor", load(dict));
assertMaglevved(load);

// And a data property again.
Object.defineProperty(dict, "target",
                      {value: "data again", writable: true,
                       enumerable: true, configurable: true});
assertEquals("data again", load(dict));

// Deleting most properties shrinks the dictionary below the cached entry.
for (let i = 0; i < 150; i++) delete dict["p" + i];
assertEquals("data again", load(dict));
assertMaglevved(load);
dict.target = "after shrink";
assertEquals("after shrink", load(dict));
assertMaglevved(load);
