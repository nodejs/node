// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that \"console.profileEnd()\" does not cause crash. (webkit:105759)");

contextGroup.addScript(`
function collectProfiles()
{
  console.profile();
  console.profile("titled");
  console.profileEnd();
  console.profileEnd();
}`);

InspectorTest.fail = function(message)
{
  InspectorTest.log("FAIL: " + message);
  InspectorTest.completeTest();
}

Protocol.Profiler.enable();
Protocol.Runtime.evaluate({ expression: "collectProfiles()"}).then(didCollectProfiles);

var headers = [];
Protocol.Profiler.onConsoleProfileFinished(function(messageObject)
{
  headers.push({
    title: messageObject["params"]["title"]
  });
});

function didCollectProfiles(messageObject)
{
  if (headers.length !== 2)
    return InspectorTest.fail("Cannot retrive headers: " + JSON.stringify(messageObject, null, 4));
  InspectorTest.log("SUCCESS: found 2 profile headers");
  for (var i = 0; i < headers.length; i++) {
    if (headers[i].title === "titled") {
      InspectorTest.log("SUCCESS: titled profile found");
      InspectorTest.completeTest();
      return;
    }
  }
  InspectorTest.fail("Cannot find titled profile");
}
