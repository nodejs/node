// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that json parser on backend correctly works with unicode');

const id = 100500;
var command = { "method": "Runtime.evaluate", "params": { expression: "\"!!!\"" }, "id": id };
session.sendRawCommand(id, JSON.stringify(command).replace("!!!", "\\u041F\\u0440\\u0438\\u0432\\u0435\\u0442 \\u043C\\u0438\\u0440"), step2);

function step2(msg)
{
  msg.id = 1;
  InspectorTest.logObject(msg);
  InspectorTest.completeTest();
}
