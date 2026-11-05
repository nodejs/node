// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
    'Untrusted sessions must not learn the isolate id from objectId');

(async function test() {
  // A fully trusted session is allowed to read the isolate id.
  const {result: {id: trustedIsolateId}} =
      await Protocol.Runtime.getIsolateId();

  // An untrusted session is denied by Runtime.getIsolateId.
  const untrustedSession = contextGroup.connect(/* isFullyTrusted */ false);
  const denied = await untrustedSession.Protocol.Runtime.getIsolateId();
  InspectorTest.log(
      'getIsolateId denied for untrusted session: ' + Boolean(denied.error));

  // Verify that objectId on untrusted sessions does not leak the isolate id.
  const {result: {result: {objectId}}} =
      await untrustedSession.Protocol.Runtime.evaluate(
          {expression: '({a: 1})'});
  const leaked = objectId.split('.')[0];
  const leakedAsHex = BigInt(leaked).toString(16);

  InspectorTest.log(
      'isolate id recovered from untrusted objectId matches getIsolateId: ' +
      (leakedAsHex === trustedIsolateId));

  // Verify that objectId operations still work within the untrusted session.
  const {result: {result: {value: propValue}}} =
      await untrustedSession.Protocol.Runtime.callFunctionOn({
        objectId,
        functionDeclaration: 'function(arg) { return this.a + arg.a; }',
        arguments: [{objectId}],
        returnByValue: true,
      });
  InspectorTest.log(
      'callFunctionOn with untrusted objectId result: ' + propValue);

  untrustedSession.disconnect();
  InspectorTest.completeTest();
})();
