// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const script = `__proto__ = Realm.global(Realm.create());`;
const w = new Worker(script, {type : 'string'});
w.postMessage('hi');
