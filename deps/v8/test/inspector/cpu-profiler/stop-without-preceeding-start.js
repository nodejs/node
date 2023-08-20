// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Test that profiler doesn't crash when we call stop without preceding start.");

Protocol.Profiler.stop().then(didStopProfile);
function didStopProfile(messageObject)
{
  expectedError("ProfileAgent.stop", messageObject);
  InspectorTest.completeTest();
}

function checkExpectation(fail, name, messageObject)
{
  if (fail === !!messageObject.error) {
    InspectorTest.log("PASS: " + name);
    return true;
  }

  InspectorTest.log("FAIL: " + name + ": " + JSON.stringify(messageObject));
  InspectorTest.completeTest();
  return false;
}
var expectedSuccess = checkExpectation.bind(null, false);
var expectedError = checkExpectation.bind(null, true);
