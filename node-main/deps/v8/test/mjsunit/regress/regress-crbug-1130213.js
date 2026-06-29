// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Realm.createAllowCrossRealmAccess();
const global = Realm.global(1);
assertSame(1, Realm.owner(global));
Realm.detachGlobal(1);
assertSame(undefined, Realm.owner(global));
