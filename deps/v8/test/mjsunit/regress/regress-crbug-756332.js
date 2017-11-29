// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  let z = ({x: {y} = {y: 42}} = {}) => y;
  assertEquals(42, z());
}

{
  let z = ({x: [y] = [42]} = {}) => y;
  assertEquals(42, z());
}
