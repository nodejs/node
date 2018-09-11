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

test(() => {
  assert_equals(self['TransformStreamDefaultController'], undefined,
                `TransformStreamDefaultController should not be defined`);
}, `TransformStreamDefaultController should not be exported on the global object`);

// Now get hold of the symbol so we can test its properties.
self.TransformStreamDefaultController = (() => {
  let controller;
  new TransformStream({
    start(c) {
      controller = c;
    }
  });
  return controller.constructor;
})();

const expected = {
  TransformStream: {
    constructor: {
      type: 'constructor',
      length: 0
    },
    readable: {
      type: 'getter'
    },
    writable: {
      type: 'getter'
    }
  },
  TransformStreamDefaultController: {
    constructor: {
      type: 'constructor',
      length: 0
    },
    desiredSize: {
      type: 'getter'
    },
    enqueue: {
      type: 'method',
      length: 1
    },
    error: {
      type: 'method',
      length: 1
    },
    terminate: {
      type: 'method',
      length: 0
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

const transformerMethods = {
  start: {
    length: 1,
    trigger: () => Promise.resolve()
  },
  transform: {
    length: 2,
    trigger: ts => ts.writable.getWriter().write()
  },
  flush: {
    length: 1,
    trigger: ts => ts.writable.getWriter().close()
  }
};

for (const method in transformerMethods) {
  const { length, trigger } = transformerMethods[method];

  // Some semantic tests of how transformer methods are called can be found in general.js, as well as in the test files
  // specific to each method.
  promise_test(() => {
    let argCount;
    const ts = new TransformStream({
      [method](...args) {
        argCount = args.length;
      }
    }, undefined, { highWaterMark: Infinity });
    return Promise.resolve(trigger(ts)).then(() => {
      assert_equals(argCount, length, `${method} should be called with ${length} arguments`);
    });
  }, `transformer method ${method} should be called with the right number of arguments`);

  promise_test(() => {
    let methodWasCalled = false;
    function Transformer() {}
    Transformer.prototype = {
      [method]() {
        methodWasCalled = true;
      }
    };
    const ts = new TransformStream(new Transformer(), undefined, { highWaterMark: Infinity });
    return Promise.resolve(trigger(ts)).then(() => {
      assert_true(methodWasCalled, `${method} should be called`);
    });
  }, `transformer method ${method} should be called even when it's located on the prototype chain`);

  promise_test(t => {
    const unreachedTraps = ['getPrototypeOf', 'setPrototypeOf', 'isExtensible', 'preventExtensions',
                            'getOwnPropertyDescriptor', 'defineProperty', 'has', 'set', 'deleteProperty', 'ownKeys',
                            'apply', 'construct'];
    const touchedProperties = [];
    const handler = {
      get: t.step_func((target, property) => {
        touchedProperties.push(property);
        if (property === 'readableType' || property === 'writableType') {
          return undefined;
        }
        return () => Promise.resolve();
      })
    };
    for (const trap of unreachedTraps) {
      handler[trap] = t.unreached_func(`${trap} should not be trapped`);
    }
    const transformer = new Proxy({}, handler);
    const ts = new TransformStream(transformer, undefined, { highWaterMark: Infinity });
    assert_array_equals(touchedProperties, ['writableType', 'readableType', 'transform', 'flush', 'start'],
                        'expected properties should be got');
    return trigger(ts).then(() => {
      assert_array_equals(touchedProperties, ['writableType', 'readableType', 'transform', 'flush', 'start'],
                          'no properties should be accessed on method call');
    });
  }, `unexpected properties should not be accessed when calling transformer method ${method}`);
}

done();
