// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that setting breakpoint before enabling debugger produces an error');

Protocol.Debugger.setBreakpointByUrl({ url: "http://example.com", lineNumber: 10  }).then(didSetBreakpointByUrlBeforeEnable);

function didSetBreakpointByUrlBeforeEnable(message)
{
  InspectorTest.log("setBreakpointByUrl error: " + JSON.stringify(message.error, null, 2));
  Protocol.Debugger.setBreakpoint().then(didSetBreakpointBeforeEnable);
}

function didSetBreakpointBeforeEnable(message)
{
  InspectorTest.log("setBreakpoint error: " + JSON.stringify(message.error, null, 2));
  InspectorTest.completeTest();
}
