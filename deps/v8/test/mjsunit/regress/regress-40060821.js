// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const obj1 = {"a":4.2, "c":0};
const obj2 = {"a":0, "b":obj1, "d":0, "e":0};
const serializer = d8.serializer;
const d = serializer.serialize(obj2);
const o = serializer.deserialize(d);
