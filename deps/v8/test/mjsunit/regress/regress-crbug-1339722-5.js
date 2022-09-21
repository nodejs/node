// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noop-on-failed-access-check

d8.file.execute('test/mjsunit/regress/regress-crbug-1321899.js');

const realm = Realm.create();
const globalProxy = Realm.global(realm);

new B(globalProxy);
B.setField(globalProxy);
assertEquals(undefined, B.getField(globalProxy));
