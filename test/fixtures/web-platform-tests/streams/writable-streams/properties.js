'use strict';

if (self.importScripts) {
  self.importScripts('/resources/testharness.js');
}

// The purpose of this file is to test for objects, attributes and arguments that should not exist.
// The test cases are generated from data tables to reduce duplication.

// Courtesy of André Bargull. Source is https://esdiscuss.org/topic/isconstructor#content-11.
function IsConstructor(o) {
  try {
    new new Proxy(o, { construct: () => ({}) })();
    return true;
  } catch (e) {
    return false;
  }
}

for (const func of ['WritableStreamDefaultController', 'WritableStreamDefaultWriter']) {
  test(() => {
    assert_equals(self[func], undefined, `${func} should not be defined`);
  }, `${func} should not be exported on the global object`);
}

// Now get hold of the symbols so we can test their properties.
self.WritableStreamDefaultController = (() => {
  let controller;
  new WritableStream({
    start(c) {
      controller = c;
    }
  });
  return controller.constructor;
})();
self.WritableStreamDefaultWriter = new WritableStream().getWriter().constructor;

const expected = {
  WritableStream: {
    constructor: {
      type: 'constructor',
      length: 0
    },
    locked: {
      type: 'getter'
    },
    abort: {
      type: 'method',
      length: 1
    },
    getWriter: {
      type: 'method',
      length: 0
    }
  },
  WritableStreamDefaultController: {
    constructor: {
      type: 'constructor',
      length: 0
    },
    error: {
      type: 'method',
      length: 1
    }
  },
  WritableStreamDefaultWriter: {
    constructor: {
      type: 'constructor',
      length: 1
    },
    closed: {
      type: 'getter'
    },
    desiredSize: {
      type: 'getter'
    },
    ready: {
      type: 'getter'
    },
    abort: {
      type: 'method',
      length: 1
    },
    close: {
      type: 'method',
      length: 0
    },
    releaseLock: {
      type: 'method',
      length: 0
    },
    write: {
      type: 'method',
      length: 1
    }
  }
};

for (const c in expected) {
  const properties = expected[c];
  const prototype = self[c].prototype;
  for (const name in properties) {
    const fullName = `${c}.prototype.${name}`;
    const descriptor = Object.getOwnPropertyDescriptor(prototype, name);
    test(() => {
      const { configurable, enumerable } = descriptor;
      assert_true(configurable, `${name} should be configurable`);
      assert_false(enumerable, `${name} should not be enumerable`);
    }, `${fullName} should have standard properties`);
    const type = properties[name].type;
    switch (type) {
      case 'getter':
        test(() => {
          const { writable, get, set } = descriptor;
          assert_equals(writable, undefined, `${name} should not be a data descriptor`);
          assert_equals(typeof get, 'function', `${name} should have a getter`);
          assert_equals(set, undefined, `${name} should not have a setter`);
        }, `${fullName} should be a getter`);
        break;

      case 'constructor':
      case 'method':
        test(() => {
          assert_true(descriptor.writable, `${name} should be writable`);
          assert_equals(typeof prototype[name], 'function', `${name} should be a function`);
          assert_equals(prototype[name].length, properties[name].length,
                        `${name} should take ${properties[name].length} arguments`);
          if (type === 'constructor') {
            assert_true(IsConstructor(prototype[name]), `${name} should be a constructor`);
            assert_equals(prototype[name].name, c, `${name}.name should be '${c}'`);
          } else {
            assert_false(IsConstructor(prototype[name]), `${name} should not be a constructor`);
            assert_equals(prototype[name].name, name, `${name}.name should be '${name}`);
          }
        }, `${fullName} should be a ${type}`);
        break;
    }
  }
  test(() => {
    const expectedPropertyNames = Object.keys(properties).sort();
    const actualPropertyNames = Object.getOwnPropertyNames(prototype).sort();
    assert_array_equals(actualPropertyNames, expectedPropertyNames,
                        `${c} properties should match expected properties`);
  }, `${c}.prototype should have exactly the expected properties`);
}

const sinkMethods = {
  start: {
    length: 1,
    trigger: () => Promise.resolve()
  },
  write: {
    length: 2,
    trigger: writer => writer.write()
  },
  close: {
    length: 0,
    trigger: writer => writer.close()
  },
  abort: {
    length: 1,
    trigger: writer => writer.abort()
  }
};

for (const method in sinkMethods) {
  const { length, trigger } = sinkMethods[method];

  // Some semantic tests of how sink methods are called can be found in general.js, as well as in the test files
  // specific to each method.
  promise_test(() => {
    let argCount;
    const ws = new WritableStream({
      [method](...args) {
        argCount = args.length;
      }
    });
    return Promise.resolve(trigger(ws.getWriter())).then(() => {
      assert_equals(argCount, length, `${method} should be called with ${length} arguments`);
    });
  }, `sink method ${method} should be called with the right number of arguments`);

  promise_test(() => {
    let methodWasCalled = false;
    function Sink() {}
    Sink.prototype = {
      [method]() {
        methodWasCalled = true;
      }
    };
    const ws = new WritableStream(new Sink());
    return Promise.resolve(trigger(ws.getWriter())).then(() => {
      assert_true(methodWasCalled, `${method} should be called`);
    });
  }, `sink method ${method} should be called even when it's located on the prototype chain`);

  promise_test(t => {
    const unreachedTraps = ['getPrototypeOf', 'setPrototypeOf', 'isExtensible', 'preventExtensions',
                            'getOwnPropertyDescriptor', 'defineProperty', 'has', 'set', 'deleteProperty', 'ownKeys',
                            'apply', 'construct'];
    const touchedProperties = [];
    const handler = {
      get: t.step_func((target, property) => {
        touchedProperties.push(property);
        if (property === 'type') {
          return undefined;
        }
        return () => Promise.resolve();
      })
    };
    for (const trap of unreachedTraps) {
      handler[trap] = t.unreached_func(`${trap} should not be trapped`);
    }
    const sink = new Proxy({}, handler);
    const ws = new WritableStream(sink);
    assert_array_equals(touchedProperties, ['type', 'write', 'close', 'abort', 'start'],
                        'expected properties should be got');
    return trigger(ws.getWriter()).then(() => {
      assert_array_equals(touchedProperties, ['type', 'write', 'close', 'abort', 'start'],
                          'no properties should be accessed on method call');
    });
  }, `unexpected properties should not be accessed when calling sink method ${method}`);
}

done();
