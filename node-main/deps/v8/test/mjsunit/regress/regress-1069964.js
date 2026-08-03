// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Realm.createAllowCrossRealmAccess();
const c = Realm.global(1);
Realm.detachGlobal(1);
try { c.constructor = () => {}; } catch {}
