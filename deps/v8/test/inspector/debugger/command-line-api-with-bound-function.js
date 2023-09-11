// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Check that debug and monitor methods from Command Line API works with bound function.");

contextGroup.addScript(`
function foo() {}
function boo() {}
var bar = boo.bind(null);

function testFunction() {
  console.log("> debug foo and bar");
  debug(foo);
  debug(bar);
  console.log("> call foo and bar");
  foo();
  bar();
  console.log("> undebug foo and bar");
  undebug(foo);
  undebug(bar);
  console.log("> call foo and bar");
  foo();
  bar();

  console.log("> monitor foo and bar");
  monitor(foo);
  monitor(bar);
  console.log("> call foo and bar");
  foo();
  bar();
  console.log("> unmonitor foo and bar");
  unmonitor(foo);
  unmonitor(bar);
  console.log("> call foo and bar");
  foo();
  bar();

  console.log("> monitor and debug bar");
  monitor(bar);
  debug(bar);
  console.log("> call bar");
  bar();
  console.log("> undebug bar");
  undebug(bar);
  console.log("> call bar");
  bar();
  console.log("> debug and unmonitor bar");
  debug(bar);
  unmonitor(bar);
  console.log("> call bar");
  bar();
}`);

Protocol.Runtime.enable();
Protocol.Debugger.enable();
Protocol.Debugger.onPaused(message => {
  var functionName = message.params.callFrames[0].functionName;
  InspectorTest.log(`paused in ${functionName}`);
  Protocol.Debugger.resume();
});
Protocol.Runtime.onConsoleAPICalled(message => InspectorTest.log(message.params.args[0].value));
Protocol.Runtime.evaluate({ expression: "testFunction()", includeCommandLineAPI: true })
  .then(InspectorTest.completeTest);
