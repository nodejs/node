// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const a = {"maxByteLength": 15061061};
const e = d8.serializer.serialize(a);
const f = new Uint8Array(e);
f[18] = 114;
assertThrows(() => { d8.serializer.deserialize(e); });
