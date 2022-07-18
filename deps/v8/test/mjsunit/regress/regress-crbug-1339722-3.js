// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --throw-on-failed-access-check

d8.file.execute('test/mjsunit/regress/regress-crbug-1321899.js');

// Attached global should have access
const realm = Realm.createAllowCrossRealmAccess();
const globalProxy = Realm.global(realm);

checkHasAccess(globalProxy);

// Access should fail after detaching
Realm.navigate(realm);

checkNoAccess(globalProxy, /Error in failed access check callback/);
