// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let obj = { 'abcdefghijklmnopqrst': 'asdf' };
let consstring_key = 'abcdefghijklmnopqrstuvwxyz'.substring(0,20);

assertTrue(obj.hasOwnProperty(consstring_key));
