// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
function LoadNodeType() {
  let node = new d8.dom.Div();

  let result = 0;
  for (var i = 0; i < 10e5; i++) {
    result += node.nodeType;
  }

  return result;
}
createSuite('nodeType', 1, LoadNodeType, ()=>{});
