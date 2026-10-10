// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-background-compile --stress-flush-code --expose-gc

let code = "globalThis.keepAlive = () => { 'use strict'; return eval('1'); }; globalThis.keepAlive();";
Realm.eval(Realm.current(), code);
for (let i = 0; i < 10; i++) gc({type: 'major'});
Realm.eval(Realm.current(), code);
