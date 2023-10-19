// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

PrivateClassMemberInspectorTest = {};

function getTestReceiver(type) {
  return type === 'private-instance-member' ? 'obj' : 'Klass';
}

function getSetupScript({ type, testRuntime }) {
  const pause = testRuntime ? '' : 'debugger;';
  if (type === 'private-instance-member' || type === 'private-static-member') {
    const isStatic = type === 'private-static-member';
    const prefix = isStatic ? 'static' : '';
    return `
class Klass {
  ${prefix} #field = "string";
  ${prefix} get #getterOnly() { return "getterOnly"; }
  ${prefix} set #setterOnly(val) { this.#field = "setterOnlyCalled"; }
  ${prefix} get #accessor() { return this.#field }
  ${prefix} set #accessor(val) { this.#field = val; }
  ${prefix} #method() { return "method"; }
}
const obj = new Klass();
${pause}`;
  }

  if (type !== 'private-conflicting-member') {
    throw new Error('unknown test type');
  }

  return `
class Klass {
  #name = "string";
}
class ClassWithField extends Klass {
  #name = "child";
}
class ClassWithMethod extends Klass {
  #name() {}
}
class ClassWithAccessor extends Klass {
  get #name() {}
  set #name(val) {}
}
class StaticClass extends Klass {
  static #name = "child";
}
${pause}`;
}

async function testAllPrivateMembers(type, runAndLog) {
  const receiver = getTestReceiver(type);
  InspectorTest.log('Checking private fields');
  await runAndLog(`${receiver}.#field`);
  await runAndLog(`${receiver}.#field = 1`);
  await runAndLog(`${receiver}.#field`);
  await runAndLog(`${receiver}.#field++`);
  await runAndLog(`${receiver}.#field`);
  await runAndLog(`++${receiver}.#field`);
  await runAndLog(`${receiver}.#field`);
  await runAndLog(`${receiver}.#field -= 3`);
  await runAndLog(`${receiver}.#field`);

  InspectorTest.log('Checking private getter-only accessors');
  await runAndLog(`${receiver}.#getterOnly`);
  await runAndLog(`${receiver}.#getterOnly = 1`);
  await runAndLog(`${receiver}.#getterOnly++`);
  await runAndLog(`${receiver}.#getterOnly -= 3`);
  await runAndLog(`${receiver}.#getterOnly`);

  InspectorTest.log('Checking private setter-only accessors');
  await runAndLog(`${receiver}.#setterOnly`);
  await runAndLog(`${receiver}.#setterOnly = 1`);
  await runAndLog(`${receiver}.#setterOnly++`);
  await runAndLog(`${receiver}.#setterOnly -= 3`);
  await runAndLog(`${receiver}.#field`);

  InspectorTest.log('Checking private accessors');
  await runAndLog(`${receiver}.#accessor`);
  await runAndLog(`${receiver}.#accessor = 1`);
  await runAndLog(`${receiver}.#field`);
  await runAndLog(`${receiver}.#accessor++`);
  await runAndLog(`${receiver}.#field`);
  await runAndLog(`++${receiver}.#accessor`);
  await runAndLog(`${receiver}.#field`);
  await runAndLog(`${receiver}.#accessor -= 3`);
  await runAndLog(`${receiver}.#field`);

  InspectorTest.log('Checking private methods');
  await runAndLog(`${receiver}.#method`);
  await runAndLog(`${receiver}.#method = 1`);
  await runAndLog(`${receiver}.#method++`);
  await runAndLog(`++${receiver}.#method`);
  await runAndLog(`${receiver}.#method -= 3`);
}

async function testConflictingPrivateMembers(runAndLog) {
  await runAndLog(`(new ClassWithField).#name`);
  await runAndLog(`(new ClassWithMethod).#name`);
  await runAndLog(`(new ClassWithAccessor).#name`);
  await runAndLog(`StaticClass.#name`);
  await runAndLog(`(new StaticClass).#name`);
}

async function runPrivateClassMemberTest(Protocol, { type, testRuntime }) {
  let runAndLog;

  if (testRuntime) {
    runAndLog = async function runAndLog(expression) {
      InspectorTest.log(`Runtime.evaluate: \`${expression}\``);
      const { result: { result } } =
        await Protocol.Runtime.evaluate({ expression, replMode: true });
      InspectorTest.logMessage(result);
    }
  } else {
    const { params: { callFrames } } = await Protocol.Debugger.oncePaused();
    const frame = callFrames[0];

    runAndLog = async function runAndLog(expression) {
      InspectorTest.log(`Debugger.evaluateOnCallFrame: \`${expression}\``);
      const { result: { result } } =
        await Protocol.Debugger.evaluateOnCallFrame({
          callFrameId: frame.callFrameId,
          expression
        });
      InspectorTest.logMessage(result);
    }
  }

  switch (type) {
    case 'private-instance-member':
    case 'private-static-member': {
      await testAllPrivateMembers(type, runAndLog);
      break;
    }
    case 'private-conflicting-member': {
      await testConflictingPrivateMembers(runAndLog);
      break;
    }
    default:
      throw new Error('unknown test type');
  }
  await Protocol.Debugger.resume();
}

PrivateClassMemberInspectorTest.runTest = function (InspectorTest, options) {
  const { contextGroup, Protocol } = InspectorTest.start(options.message);

  if (options.testRuntime) {
    Protocol.Runtime.enable();
  } else {
    Protocol.Debugger.enable();
  }
  const source = getSetupScript(options);
  InspectorTest.log(source);
  if (options.module) {
    contextGroup.addModule(source, 'module');
  } else {
    contextGroup.addScript(source);
  }

  InspectorTest.runAsyncTestSuite([async function evaluatePrivateMembers() {
    await runPrivateClassMemberTest(Protocol, options);
  }]);
}

async function printPrivateMembers(Protocol, InspectorTest, options) {
  let { result } = await Protocol.Runtime.getProperties(options);
  InspectorTest.log('privateProperties from Runtime.getProperties()');
  if (result.privateProperties === undefined) {
    InspectorTest.logObject(result.privateProperties);
  } else {
    InspectorTest.logMessage(result.privateProperties);
  }

  // Can happen to accessorPropertiesOnly requests.
  if (result.internalProperties === undefined) {
    return;
  }

  InspectorTest.log('[[PrivateMethods]] in internalProperties from Runtime.getProperties()');
  let privateMethods = result.internalProperties.find((i) => i.name === '[[PrivateMethods]]');
  if (privateMethods === undefined) {
    InspectorTest.logObject(privateMethods);
    return;
  }

  InspectorTest.logMessage(privateMethods);
  ({ result } = await Protocol.Runtime.getProperties({
    objectId: privateMethods.value.objectId
  }));
  InspectorTest.logMessage(result);
}
