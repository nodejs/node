// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let called_custom_unicode_getter = false;
const re = /./;

function f() {
  re.__defineGetter__("unicode", function() {
    called_custom_unicode_getter = true;
  });
  return 2;
}

assertEquals(["","",], re[Symbol.split]("abc", { valueOf: f }));

// The spec mandates retrieving the regexp instance's flags before
// ToUint(limit), i.e. the unicode getter must still be unmodified when
// flags are retrieved.
assertFalse(called_custom_unicode_getter);
