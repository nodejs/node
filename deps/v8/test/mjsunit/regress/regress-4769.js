// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// https://bugs.chromium.org/p/v8/issues/detail?id=4769

Object.getPrototypeOf([])[Symbol.iterator] = () => assertUnreachable();

JSON.stringify({foo: [42]});
JSON.stringify({foo: [42]}, []);
JSON.stringify({foo: [42]}, undefined, ' ');
JSON.stringify({foo: [42]}, [], ' ');
