// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Test that profiling can only be started when Profiler was enabled and that Profiler.disable command will stop recording all profiles.");

Protocol.Profiler.start().then(didFailToStartWhenDisabled);
disallowConsoleProfiles();

function disallowConsoleProfiles()
{
  Protocol.Profiler.onConsoleProfileStarted(function(messageObject)
  {
    InspectorTest.log("FAIL: console profile started " + JSON.stringify(messageObject, null, 4));
  });
  Protocol.Profiler.onConsoleProfileFinished(function(messageObject)
  {
    InspectorTest.log("FAIL: unexpected profile received " + JSON.stringify(messageObject, null, 4));
  });
}
function allowConsoleProfiles()
{
  Protocol.Profiler.onConsoleProfileStarted(function(messageObject)
  {
    InspectorTest.log("PASS: console initiated profile started");
  });
  Protocol.Profiler.onConsoleProfileFinished(function(messageObject)
  {
    InspectorTest.log("PASS: console initiated profile received");
  });
}
function didFailToStartWhenDisabled(messageObject)
{
  if (!expectedError("didFailToStartWhenDisabled", messageObject))
    return;
  allowConsoleProfiles();
  Protocol.Profiler.enable();
  Protocol.Profiler.start().then(didStartFrontendProfile);
}
function didStartFrontendProfile(messageObject)
{
  if (!expectedSuccess("didStartFrontendProfile", messageObject))
    return;
  Protocol.Runtime.evaluate({expression: "console.profile('p1');"}).then(didStartConsoleProfile);
}

function didStartConsoleProfile(messageObject)
{
  if (!expectedSuccess("didStartConsoleProfile", messageObject))
    return;
  Protocol.Profiler.disable().then(didDisableProfiler);
}

function didDisableProfiler(messageObject)
{
  if (!expectedSuccess("didDisableProfiler", messageObject))
    return;
  Protocol.Profiler.enable();
  Protocol.Profiler.stop().then(didStopFrontendProfile);
}

function didStopFrontendProfile(messageObject)
{
  if (!expectedError("no front-end initiated profiles found", messageObject))
    return;
  disallowConsoleProfiles();
  Protocol.Runtime.evaluate({expression: "console.profileEnd();"}).then(didStopConsoleProfile);
}

function didStopConsoleProfile(messageObject)
{
  if (!expectedSuccess("didStopConsoleProfile", messageObject))
    return;
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
