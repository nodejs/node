// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Regression test for issue 2318: evaluate near stack overflow.');

const source = `
function f() {
  var i = 0;
  // Stack-allocate to reach the end of stack quickly.
  var _A0 = 00; var _A1 = 01; var _A2 = 02; var _A3 = 03; var _A4 = 04;
  var _B0 = 05; var _B1 = 06; var _B2 = 07; var _B3 = 08; var _B4 = 09;
  var _C0 = 10; var _C1 = 11; var _C2 = 12; var _C3 = 13; var _C4 = 14;
  var _D0 = 15; var _D1 = 16; var _D2 = 17; var _D3 = 18; var _D4 = 19;
  var _E0 = 20; var _E1 = 21; var _E2 = 22; var _E3 = 23; var _E4 = 24;
  var _F0 = 25; var _F1 = 26; var _F2 = 27; var _F3 = 28; var _F4 = 29;
  var _G0 = 30; var _G1 = 31; var _G2 = 32; var _G3 = 33; var _G4 = 34;
  var _H0 = 35; var _H1 = 36; var _H2 = 37; var _H3 = 38; var _H4 = 39;
  var _I0 = 40; var _I1 = 41; var _I2 = 42; var _I3 = 43; var _I4 = 44;
  var _J0 = 45; var _J1 = 46; var _J2 = 47; var _J3 = 48; var _J4 = 49;
  var _K0 = 50; var _K1 = 51; var _K2 = 52; var _K3 = 53; var _K4 = 54;
  var _L0 = 55; var _L1 = 56; var _L2 = 57; var _L3 = 58; var _L4 = 59;
  var _M0 = 60; var _M1 = 61; var _M2 = 62; var _M3 = 63; var _M4 = 64;
  var _N0 = 65; var _N1 = 66; var _N2 = 67; var _N3 = 68; var _N4 = 69;
  var _O0 = 70; var _O1 = 71; var _O2 = 72; var _O3 = 73; var _O4 = 74;
  var _P0 = 75; var _P1 = 76; var _P2 = 77; var _P3 = 78; var _P4 = 79;
  var _Q0 = 80; var _Q1 = 81; var _Q2 = 82; var _Q3 = 83; var _Q4 = 84;
  var _R0 = 85; var _R1 = 86; var _R2 = 87; var _R3 = 88; var _R4 = 89;
  var _S0 = 90; var _S1 = 91; var _S2 = 92; var _S3 = 93; var _S4 = 94;
  var _T0 = 95; var _T1 = 96; var _T2 = 97; var _T3 = 98; var _T4 = 99;

  f();
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let pausedCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    pausedCount++;
    await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: callFrames[0].callFrameId, expression: 'i'});
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await Protocol.Debugger.setBreakpointByUrl({url: 'test.js', lineNumber: 2});

  let res = await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log('Paused count > 0: ' + (pausedCount > 0));
  InspectorTest.log('Has exception: ' + !!res.result.exceptionDetails);
  if (res.result.exceptionDetails) {
    InspectorTest.log(
        'Exception: ' + res.result.exceptionDetails.exception.className);
  }
  InspectorTest.completeTest();
})();
