// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle-reprl

// JS_BUNDLE_SCRIPT
Realm.shared = {value: 42};
print("part 1:", Realm.shared.value);

// JS_BUNDLE_SCRIPT
// Realm.shared does not leak from part 1.
print("part 2:", typeof Realm.shared);
