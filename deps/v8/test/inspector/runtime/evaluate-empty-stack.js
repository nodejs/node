// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that Runtime.evaluate works with an empty stack");

contextGroup.addScript("var text = [48116210, 34460128, 1406661984071834]");

var message = { expression: "text.map(x => x.toString(36)).join(' ')" };

Protocol.Runtime.evaluate(message)
  .then(message => InspectorTest.logMessage(message))
  .then(() => InspectorTest.completeTest());
