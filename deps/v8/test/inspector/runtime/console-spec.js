// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests console object and it\'s prototype');

contextGroup.addScript(`
var self = this;
function checkPrototype() {
  const prototype1 = Object.getPrototypeOf(console);
  const prototype2 = Object.getPrototypeOf(prototype1);
  if (Object.getOwnPropertyNames(prototype1).length !== 0)
    return "false: The [[Prototype]] must have no properties";
  if (prototype2 !== Object.prototype)
    return "false: The [[Prototype]]'s [[Prototype]] must be %ObjectPrototype%";
  return "true";
}
`);

InspectorTest.runAsyncTestSuite([
  async function consoleExistsOnGlobal() {
    let message = await Protocol.Runtime.evaluate({
      expression: 'self.hasOwnProperty(\'console\')', returnByValue: true});
    InspectorTest.log(message.result.result.value);
  },

  async function consoleHasRightPropertyDescriptor() {
    let message = await Protocol.Runtime.evaluate({
      expression: 'Object.getOwnPropertyDescriptor(self, \'console\')',
      returnByValue: true});
    let result = message.result.result.value;
    result.value = '<value>';
    InspectorTest.logObject(result);
  },

  async function ConsoleNotExistsOnGlobal() {
    let message = await Protocol.Runtime.evaluate({
      expression: '\'Console\' in self', returnByValue: true})
    InspectorTest.log(message.result.result.value);
  },

  async function prototypeChainMustBeCorrect() {
    let message = await Protocol.Runtime.evaluate({
      expression: "checkPrototype()", returnByValue: true });
    InspectorTest.log(message.result.result.value);
  },

  async function consoleToString() {
    let message = await Protocol.Runtime.evaluate({
      expression: 'console.toString()', returnByValue: true})
    InspectorTest.log(message.result.result.value);
  },

  async function consoleMethodPropertyDescriptor() {
    let message = await Protocol.Runtime.evaluate({
      expression: 'Object.getOwnPropertyDescriptor(console, \'log\')',
      returnByValue: true});
    InspectorTest.logObject(message.result.result.value);
  }
]);
