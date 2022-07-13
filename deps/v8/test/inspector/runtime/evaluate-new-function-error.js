// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that Runtime.evaluate has the correct error line number for 'new Function(...)'");

var message = { expression: "new Function('(0)()')();" };

Protocol.Runtime.evaluate(message)
  .then(message => InspectorTest.logMessage(message))
  .then(() => InspectorTest.completeTest());
