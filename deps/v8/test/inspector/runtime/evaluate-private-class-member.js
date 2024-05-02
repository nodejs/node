// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/private-class-member-inspector-test.js');

const options = {
  type: 'private-instance-member',
  testRuntime: true,
  message: `Evaluate private class member out of class scope in Runtime.evaluate()`
};
PrivateClassMemberInspectorTest.runTest(InspectorTest, options);
