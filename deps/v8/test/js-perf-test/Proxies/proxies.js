// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function newBenchmark(name, handlers) {
  new BenchmarkSuite(name, [1000], [
    new Benchmark(name, false, false, 0,
                  handlers.run, handlers.setup, handlers.teardown)
  ]);
}

// ----------------------------------------------------------------------------

var obj;
var p;
var result;
class Class {};
const symbol = Symbol();

const SOME_NUMBER = 42;
const SOME_OTHER_NUMBER = 1337;
const ITERATIONS = 1000;

newBenchmark("CallProxyWithoutTrap", {
  setup() {
    const target = () => { return SOME_NUMBER; };
    p = new Proxy(target, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = p();
    }
  },
  teardown() {
    assert(result === SOME_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("CallProxyWithTrap", {
  setup() {
    const target = () => { return SOME_NUMBER; };
    p = new Proxy(target, {
      apply: function(target, thisArg, argumentsList) {
        return SOME_OTHER_NUMBER;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = p();
    }
  },
  teardown() {
    assert(result === SOME_OTHER_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("ConstructProxyWithoutTrap", {
  setup() {
    p = new Proxy(Class, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = new p();
    }
  },
  teardown() {
    assert(result instanceof Class, `result not an instance of class`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("ConstructProxyWithTrap", {
  setup() {
    p = new Proxy(Object, {
      construct: function(target, argumentsList, newTarget) {
        return new Class;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = new p();
    }
  },
  teardown() {
    assert(result instanceof Class, `result not an instance of class`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetStringWithoutTrap", {
  setup() {
    p = new Proxy({
      prop: SOME_NUMBER
    }, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = p.prop;
    }
  },
  teardown() {
    assert(result === SOME_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetStringWithTrap", {
  setup() {
    p = new Proxy({
      prop: SOME_NUMBER
    }, {
      get: function(target, propertyKey, receiver) {
        return SOME_OTHER_NUMBER;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = p.prop;
    }
  },
  teardown() {
    assert(result === SOME_OTHER_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetIndexWithoutTrap", {
  setup() {
    p = new Proxy([SOME_NUMBER], {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = p[0];
    }
  },
  teardown() {
    assert(result === SOME_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetIndexWithTrap", {
  setup() {
    p = new Proxy([SOME_NUMBER], {
      get: function(target, propertyKey, receiver) {
        return SOME_OTHER_NUMBER;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = p[0];
    }
  },
  teardown() {
    assert(result === SOME_OTHER_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetSymbolWithoutTrap", {
  setup() {
    p = new Proxy({[symbol]: SOME_NUMBER}, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = p[symbol];
    }
  },
  teardown() {
    assert(result === SOME_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetSymbolWithTrap", {
  setup() {
    p = new Proxy({[symbol]: SOME_NUMBER}, {
      get: function(target, propertyKey, receiver) {
        return SOME_OTHER_NUMBER;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = p[symbol];
    }
  },
  teardown() {
    assert(result === SOME_OTHER_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("HasStringWithoutTrap", {
  setup() {
    p = new Proxy({prop: 42}, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = ('prop' in p);
    }
  },
  teardown() {
    assert(result === true, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("HasStringWithTrap", {
  setup() {
    p = new Proxy({}, {
      has: function(target, propertyKey) {
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = ('prop' in p);
    }
  },
  teardown() {
    assert(result === true, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("HasSymbolWithoutTrap", {
  setup() {
    p = new Proxy({[symbol]: SOME_NUMBER}, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = (symbol in p);
    }
  },
  teardown() {
    assert(result === true, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("HasSymbolWithTrap", {
  setup() {
    p = new Proxy({}, {
      has: function(target, propertyKey) {
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = (symbol in p);
    }
  },
  teardown() {
    assert(result === true, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetStringWithoutTrap", {
  setup() {
    obj = {prop: 0};
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p.prop = SOME_NUMBER;
    }
  },
  teardown() {
    assert(obj.prop === SOME_NUMBER, `wrong result: ${obj.prop}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetStringWithTrap", {
  setup() {
    obj = {prop: 0};
    p = new Proxy(obj, {
      set: function(target, propertyKey, value, receiver) {
        target[propertyKey] = SOME_OTHER_NUMBER;
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p.prop = SOME_NUMBER;
    }
  },
  teardown() {
    assert(obj.prop === SOME_OTHER_NUMBER, `wrong result: ${obj.prop}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetIndexWithoutTrap", {
  setup() {
    obj = [undefined];
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p[0] = SOME_NUMBER;
    }
  },
  teardown() {
    assert(obj[0] === SOME_NUMBER, `wrong result: ${obj[0]}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetIndexWithTrap", {
  setup() {
    obj = [undefined];
    p = new Proxy(obj, {
      set: function(target, propertyKey, value, receiver) {
        target[propertyKey] = SOME_OTHER_NUMBER;
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p[0] = SOME_NUMBER;
    }
  },
  teardown() {
    assert(obj[0] === SOME_OTHER_NUMBER, `wrong result: ${obj[0]}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetSymbolWithoutTrap", {
  setup() {
    obj = {[symbol]: 0};
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p[symbol] = SOME_NUMBER;
    }
  },
  teardown() {
    assert(obj[symbol] === SOME_NUMBER, `wrong result: ${obj[symbol]}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetSymbolWithTrap", {
  setup() {
    obj = {[symbol]: 0};
    p = new Proxy(obj, {
      set: function(target, propertyKey, value, receiver) {
        target[propertyKey] = SOME_OTHER_NUMBER;
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p[symbol] = SOME_NUMBER;
    }
  },
  teardown() {
    assert(obj[symbol] === SOME_OTHER_NUMBER, `wrong result: ${obj[symbol]}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("HasInIdiom", {
  setup() {
    obj = {};
    for (var i = 0; i < 20; ++i) {
      obj['prop' + i] = SOME_OTHER_NUMBER;
    }
    p = new Proxy(obj, {
      has: function(target, propertyKey) {
        return true;
      },
      get: function(target, propertyKey, receiver) {
        if (typeof propertyKey == 'string' && propertyKey.match('prop')) {
          return SOME_NUMBER;
        } else {
          throw new Error(`Unexpected property access: ${propertyKey}`);
        }
      },
    });
  },
  run() {
    var o = p;
    var sum = 0;
    for (var x in o) {
      if (Object.hasOwnProperty.call(o, x)) {
        sum += o[x];
      }
    }
    result = sum;
  },
  teardown() {
    assert(result === 20 * SOME_NUMBER, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("IsExtensibleWithoutTrap", {
  setup() {
    p = new Proxy({}, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = Object.isExtensible(p);
    }
  },
  teardown() {
    assert(result === true, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("IsExtensibleWithTrap", {
  setup() {
    p = new Proxy({}, {
      isExtensible: function(target) {
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = Object.isExtensible(p);
    }
  },
  teardown() {
    assert(result === true, `wrong result: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("PreventExtensionsWithoutTrap", {
  setup() {
    p = new Proxy({}, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = Object.preventExtensions(p);
    }
  },
  teardown() {}
});

// ----------------------------------------------------------------------------

newBenchmark("PreventExtensionsWithTrap", {
  setup() {
    p = new Proxy({}, {
      preventExtensions: function(target) {
        Object.preventExtensions(target);
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = Object.preventExtensions(p);
    }
  },
  teardown() {}
});

// ----------------------------------------------------------------------------

newBenchmark("GetPrototypeOfWithoutTrap", {
  setup() {
    p = new Proxy({}, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = Object.getPrototypeOf(p);
    }
  },
  teardown() {
    assert(result === Object.prototype, `wrong prototype: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetPrototypeOfWithTrap", {
  setup() {
    p = new Proxy({}, {
      getPrototypeOf: function(target) {
        return Array.prototype;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = Object.getPrototypeOf(p);
    }
  },
  teardown() {
    assert(result === Array.prototype, `wrong prototype: ${result}`);
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetPrototypeOfWithoutTrap", {
  setup() {
    var obj = { x: 1 };
    obj.__proto__ = {};
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = Object.setPrototypeOf(p, [1]);
    }
  },
  teardown() {}
});

// ----------------------------------------------------------------------------

newBenchmark("SetPrototypeOfWithTrap", {
  setup() {
    var obj = { x: 1 };
    obj.__proto__ = {};
    p = new Proxy(obj, {
      setPrototypeOf: function(target, proto) {
        Object.setPrototypeOf(target, proto);
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      result = Object.setPrototypeOf(p, [1]);
    }
  },
  teardown() {}
});
