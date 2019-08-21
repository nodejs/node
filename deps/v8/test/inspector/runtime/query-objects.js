// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
  InspectorTest.start('Checks Runtime.queryObjects');

InspectorTest.runAsyncTestSuite([
  async function testClass() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    InspectorTest.log('Declare class Foo & store its constructor.');
    await Protocol.Runtime.evaluate({
      expression: 'class Foo{constructor(){}};'
    });
    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'Foo.prototype'
    });

    for (let i = 0; i < 2; ++i) {
      InspectorTest.log('Create object with class Foo.');
      Protocol.Runtime.evaluate({expression: 'new Foo()'});
      await queryObjects(session, objectId, 'Foo');
    }

    session.disconnect();
  },

  async function testDerivedNewClass() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    InspectorTest.log('Declare class Foo & store its constructor.');
    Protocol.Runtime.evaluate({expression: 'class Foo{};'});
    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'Foo.prototype'
    });
    let fooConstructorId = objectId;

    InspectorTest.log('Declare class Boo extends Foo & store its constructor.');
    Protocol.Runtime.evaluate({expression: 'class Boo extends Foo{};'});
    ({result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'Boo.prototype'
    }));
    let booConstructorId = objectId;

    await queryObjects(session, fooConstructorId, 'Foo');
    await queryObjects(session, booConstructorId, 'Boo');

    InspectorTest.log('Create object with class Foo');
    Protocol.Runtime.evaluate({expression: 'new Foo()'});
    await queryObjects(session, fooConstructorId, 'Foo');

    InspectorTest.log('Create object with class Boo');
    Protocol.Runtime.evaluate({expression: 'new Boo()'});
    await queryObjects(session, fooConstructorId, 'Foo');
    await queryObjects(session, booConstructorId, 'Boo');

    session.disconnect();
  },

  async function testNewFunction() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    InspectorTest.log('Declare Foo & store it.');
    Protocol.Runtime.evaluate({expression: 'function Foo(){}'});
    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'Foo.prototype'
    });

    for (let i = 0; i < 2; ++i) {
      InspectorTest.log('Create object using Foo.');
      Protocol.Runtime.evaluate({expression: 'new Foo()'});
      await queryObjects(session, objectId, 'Foo');
    }
    session.disconnect();
  },

  async function testNonInspectable() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    InspectorTest.log('Declare Foo & store it.');
    Protocol.Runtime.evaluate({expression: 'function Foo(){}'});
    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'Foo.prototype'
    });

    InspectorTest.log('Create object using Foo.');
    Protocol.Runtime.evaluate({expression: 'a = new Foo()'});
    await queryObjects(session, objectId, 'Foo');
    InspectorTest.log('Mark object as not inspectable.')
    Protocol.Runtime.evaluate({expression: 'inspector.markObjectAsNotInspectable(a)'});
    await queryObjects(session, objectId, 'Foo');
    session.disconnect();
  },

  async function testObjectCreate() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    InspectorTest.log('Declare Object p & store it.');
    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'p = {a:1}'
    });
    for (let i = 0; i < 2; ++i) {
      InspectorTest.log('Create object using Object.create(p).');
      Protocol.Runtime.evaluate({expression: 'Object.create(p)'});
      await queryObjects(session, objectId, 'p');
    }
    session.disconnect();
  },

  async function testQueryObjectsWithFeedbackVector() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;

    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'Object.prototype',
    });
    let countBefore = await countObjects(session, objectId);
    await Protocol.Runtime.evaluate({
      returnByValue: true,
      expression: `
        global.dummyFunction = () => {
          [42];
          {foo: 'bar'};
          [1,2,3];
        }
        %EnsureFeedbackVectorForFunction(dummyFunction);
        dummyFunction();
      `
    });
    let countAfter = await countObjects(session, objectId);
    // Difference should be 1 since |dummyFunction| is retained.
    InspectorTest.log('Before/After difference: ' + (countAfter - countBefore));
    session.disconnect();
  },

  async function testWithObjectGroup() {
    let contextGroup = new InspectorTest.ContextGroup();
    let session = contextGroup.connect();
    let Protocol = session.Protocol;
    let {result:{result:{objectId}}} = await Protocol.Runtime.evaluate({
      expression: 'Array.prototype'
    });

    let initialArrayCount;
    const N = 3;
    InspectorTest.log(`Query for Array.prototype ${N} times`);
    for (let i = 0; i < N; ++i) {
      const {result:{objects}} = await Protocol.Runtime.queryObjects({
        prototypeObjectId: objectId,
        objectGroup: 'console'
      });
      logCountSinceInitial(objects.description);
    }

    await Protocol.Runtime.releaseObjectGroup({objectGroup: 'console'});
    InspectorTest.log('\nReleased object group.');

    const {result:{objects}} = await Protocol.Runtime.queryObjects({
      prototypeObjectId: objectId,
      objectGroup: 'console'
    });
    logCountSinceInitial(objects.description);
    session.disconnect();

    function logCountSinceInitial(description) {
      const count = parseInt(/Array\((\d+)\)/.exec(description)[1]);
      if (typeof initialArrayCount === 'undefined')
        initialArrayCount = count;
      InspectorTest.logMessage(`Results since initial: ${count - initialArrayCount}`);
    }
  },
]);

const constructorsNameFunction = `
function() {
  return this.map(o => o.constructor.name + ',' + typeof o).sort();
}`;

async function queryObjects(sesion, prototypeObjectId, name) {
  let {result:{objects}} = await sesion.Protocol.Runtime.queryObjects({
    prototypeObjectId
  });
  InspectorTest.log(`Query objects with ${name} prototype.`);
  let {result:{result:{value}}} = await sesion.Protocol.Runtime.callFunctionOn({
    objectId: objects.objectId,
    functionDeclaration: constructorsNameFunction,
    returnByValue: true
  });
  InspectorTest.log('Dump each object constructor name.');
  InspectorTest.logMessage(value);
}

async function countObjects(session, prototypeObjectId) {
  let {result:{objects}} = await session.Protocol.Runtime.queryObjects({
    prototypeObjectId
  });
  let {result:{result:{value}}} = await session.Protocol.Runtime.callFunctionOn({
    objectId: objects.objectId,
    functionDeclaration: `function() { return this.length; }`,
    returnByValue: true
  });
  await session.Protocol.Runtime.releaseObject({
    objectId: objects.objectId,
  });
  return value;
}
