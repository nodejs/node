// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Test that profiler is able to record a profile. Also it tests that profiler returns an error when it unable to find the profile.");

Protocol.Profiler.enable();
Protocol.Profiler.start().then(didStartFrontendProfile);
function didStartFrontendProfile(messageObject)
{
  if (!expectedSuccess("startFrontendProfile", messageObject))
    return;
  Protocol.Runtime.evaluate({expression: "console.profile('Profile 1');"}).then(didStartConsoleProfile);
}

function didStartConsoleProfile(messageObject)
{
  if (!expectedSuccess("startConsoleProfile", messageObject))
    return;
  Protocol.Runtime.evaluate({expression: "console.profileEnd('Profile 1');"}).then(didStopConsoleProfile);
}

function didStopConsoleProfile(messageObject)
{
  if (!expectedSuccess("stopConsoleProfile", messageObject))
    return;
  Protocol.Profiler.stop().then(didStopFrontendProfile);
}

function didStopFrontendProfile(messageObject)
{
  if (!expectedSuccess("stoppedFrontendProfile", messageObject))
    return;
  Protocol.Profiler.start().then(didStartFrontendProfile2);
}

function didStartFrontendProfile2(messageObject)
{
  if (!expectedSuccess("startFrontendProfileSecondTime", messageObject))
    return;
  Protocol.Profiler.stop().then(didStopFrontendProfile2);
}

function didStopFrontendProfile2(messageObject)
{
  expectedSuccess("stopFrontendProfileSecondTime", messageObject)
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
