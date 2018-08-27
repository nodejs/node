// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start(
    'Tests that microtasks run before the Runtime.evaluate response is sent');

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(InspectorTest.logMessage);
Protocol.Runtime
    .evaluate({expression: 'Promise.resolve().then(() => console.log(42))'})
    .then(InspectorTest.logMessage)
    .then(InspectorTest.completeTest);
