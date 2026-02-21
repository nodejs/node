// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start('Recursive proxy prototype does not crash inspector crbug.com/1080638');

const reproductionCode = `
const t = { id: 1 }
const p = new Proxy(t, {
  get(target, prop, receiver) {
    console.log(receiver);
    return Reflect.get(target, prop);
  }
});

const q = Object.create(p);
console.log(q.id);
`;

(async function logPropertyWithProxyPrototype() {
  await Protocol.Runtime.enable();
  const response = await Protocol.Runtime.evaluate({
    expression: reproductionCode,
    replMode: true,
  });
  InspectorTest.logMessage(response);
  InspectorTest.completeTest();
})();
