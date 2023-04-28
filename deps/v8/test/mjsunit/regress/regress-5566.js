// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://github.com/tc39/proposal-regexp-legacy-features#additional-properties-of-the-regexp-constructor

const props = [ "input", "$_"
              , "lastMatch", "$&"
              , "lastParen", "$+"
              , "leftContext", "$`"
              , "rightContext", "$'"
              , "$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9"
              ];

for (let i = 0; i < props.length; i++) {
  const prop = props[i];
  const desc = Object.getOwnPropertyDescriptor(RegExp, prop);
  assertTrue(desc.configurable, prop);
  assertFalse(desc.enumerable, prop);
  assertTrue(desc.get !== undefined, prop);

  // TODO(jgruber): Although the spec proposal specifies setting setters to
  // undefined, we are not sure that this change would be web-compatible, and
  // we are intentionally sticking with the old behavior for now.
  assertTrue(desc.set !== undefined, prop);
}
