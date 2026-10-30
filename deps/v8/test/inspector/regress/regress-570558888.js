// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/570558888.');

contextGroup.addScript(
    `function donor(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,aa,bb,cc) {
  return corrupt();
}
function victim() {
  corrupt();
  return 0;
}`,
    0, 0, 'test.js');

contextGroup.addScript(`
function corrupt() {
  const donor_sfi = Sandbox.readObjectField(donor, 'shared_function_info');
  Sandbox.corruptObjectField(victim, 'shared_function_info', donor_sfi);
}

function trigger() {
  victim();
}
`);

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 1,
    columnNumber: 0,
  });
  await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 5,
    columnNumber: 0,
  });
  await Protocol.Runtime.evaluate({expression: 'trigger()'});
  InspectorTest.completeTest();
})();
