// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/private-class-member-inspector-test.js');

const options = {
  type: 'private-static-member',
  testRuntime: false,
  message: `Evaluate static private class member out of class scope in Debugger.evaluateOnCallFrame()`
};
PrivateClassMemberInspectorTest.runTest(InspectorTest, options);
