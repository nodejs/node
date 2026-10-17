// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const key1 = String.fromCharCode(49, 48, 48, 48, 0x130, 48, 48, 48, 48, 48)
                 .replace(/(\u0130)()/g, '$2');
Symbol.for(key1);
const object1 =
    (0, eval)(`({${key1}(v){return Object.values({"${key1}":v})[1]}})`);
assertEquals(undefined, object1[key1](1));

const key2 = String.fromCharCode(49, 48, 48, 48, 0x130, 48, 48, 48, 48, 49)
                 .replace(/(\u0130)()/g, '$2');
new Set().add(key2);
Symbol.for(key2);
const object2 =
    (0, eval)(`({${key2}(v){return Object.values({"${key2}":v})[1]}})`);
assertEquals(undefined, object2[key2](1));
