// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests that Runtime.getExceptionDetails works');

const expression = `
function foo() {
  return new Error("error 1");
}
foo();
`;

const expressionWithMeta = `
function foo() {
  return new inspector.newExceptionWithMetaData('myerror', 'foo', 'bar');
}
foo();
`;

InspectorTest.runAsyncTestSuite([
  async function itShouldReturnExceptionDetailsForJSErrorObjects() {
    await Protocol.Runtime.enable();  // Enable detailed stacktrace capturing.
    const {result} = await Protocol.Runtime.evaluate({expression});
    InspectorTest.logMessage(await Protocol.Runtime.getExceptionDetails(
        {errorObjectId: result.result.objectId}));
  },

  async function itShouldReturnIncompleteDetailsForJSErrorWithNoStack() {
    await Protocol.Runtime.disable();  // Disable detailed stacktrace capturing.
    const {result} = await Protocol.Runtime.evaluate({expression});
    InspectorTest.logMessage(await Protocol.Runtime.getExceptionDetails(
        {errorObjectId: result.result.objectId}));
  },

  async function itShouldReportAnErrorForNonJSErrorObjects() {
    const {result} = await Protocol.Runtime.evaluate({expression: '() =>({})'});
    InspectorTest.logMessage(await Protocol.Runtime.getExceptionDetails(
        {errorObjectId: result.result.objectId}));
  },

  async function itShouldIncludeMetaData() {
    await Protocol.Runtime.enable();  // Enable detailed stacktrace capturing.
    const {result} = await Protocol.Runtime.evaluate({expression: expressionWithMeta});
    InspectorTest.logMessage(await Protocol.Runtime.getExceptionDetails(
      {errorObjectId: result.result.objectId}));
  }
]);
