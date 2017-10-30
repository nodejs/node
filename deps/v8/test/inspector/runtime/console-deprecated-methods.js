// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests checks that deprecation messages for console.")

Protocol.Runtime.onConsoleAPICalled(messageAdded);
Protocol.Runtime.enable();

var deprecatedMethods = [
  "console.timeline(\"42\")",
  "console.timeline(\"42\")",
  "console.timeline(\"42\")", // three calls should produce one warning message
  "console.timelineEnd(\"42\")",
  "console.markTimeline(\"42\")",
];
Protocol.Runtime.evaluate({ expression: deprecatedMethods.join(";") });

var messagesLeft = 3;
function messageAdded(data)
{
  var text = data.params.args[0].value;
  if (text.indexOf("deprecated") === -1)
    return;
  InspectorTest.log(text);
  if (!--messagesLeft)
    InspectorTest.completeTest();
}
