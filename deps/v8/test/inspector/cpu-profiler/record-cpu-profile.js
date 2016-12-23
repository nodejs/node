// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

print("Test that profiler is able to record a profile. Also it tests that profiler returns an error when it unable to find the profile.");

Protocol.Profiler.enable();
Protocol.Profiler.start().then(didStartFrontendProfile);
function didStartFrontendProfile(messageObject)
{
  if (!InspectorTest.expectedSuccess("startFrontendProfile", messageObject))
    return;
  Protocol.Runtime.evaluate({expression: "console.profile('Profile 1');"}).then(didStartConsoleProfile);
}

function didStartConsoleProfile(messageObject)
{
  if (!InspectorTest.expectedSuccess("startConsoleProfile", messageObject))
    return;
  Protocol.Runtime.evaluate({expression: "console.profileEnd('Profile 1');"}).then(didStopConsoleProfile);
}

function didStopConsoleProfile(messageObject)
{
  if (!InspectorTest.expectedSuccess("stopConsoleProfile", messageObject))
    return;
  Protocol.Profiler.stop().then(didStopFrontendProfile);
}

function didStopFrontendProfile(messageObject)
{
  if (!InspectorTest.expectedSuccess("stoppedFrontendProfile", messageObject))
    return;
  Protocol.Profiler.start().then(didStartFrontendProfile2);
}

function didStartFrontendProfile2(messageObject)
{
  if (!InspectorTest.expectedSuccess("startFrontendProfileSecondTime", messageObject))
    return;
  Protocol.Profiler.stop().then(didStopFrontendProfile2);
}

function didStopFrontendProfile2(messageObject)
{
  InspectorTest.expectedSuccess("stopFrontendProfileSecondTime", messageObject)
  InspectorTest.completeTest();
}
