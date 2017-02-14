// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var messages = [];

function messageAdded(data)
{
  var payload = data.params;
  if (messages.length > 0)
    InspectorTest.log("Message " + messages.length + " has non-decreasing timestamp: " + (payload.timestamp >= messages[messages.length - 1].timestamp));

  messages.push(payload);
  InspectorTest.log("Message has timestamp: " + !!payload.timestamp);

  InspectorTest.log("Message timestamp doesn't differ too much from current time (one minute interval): " + (Math.abs(new Date().getTime() - payload.timestamp) < 60000));
  if (messages.length === 3)
    InspectorTest.completeTest();
}

Protocol.Runtime.onConsoleAPICalled(messageAdded);
Protocol.Runtime.enable();
Protocol.Runtime.evaluate({ expression: "console.log('testUnique'); for (var i = 0; i < 2; ++i) console.log('testDouble');" });
