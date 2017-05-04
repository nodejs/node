// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Check destroying agent inside of breakProgram');

(async function test(){
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'inspector.breakProgram(\'\', \'{}\')'});
  await Protocol.Debugger.oncePaused();
  utils.disconnect();
  utils.quit();
})();
