// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string

// Attempt to externalize a string that's in RO_SPACE, which is not allowed as
// the string's map would need to be writable.
assertThrows(() => {
  externalizeString("1", false)
});
assertThrows(() => {
  externalizeString("1", true)
});
