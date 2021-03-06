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

var result;
var foo = () => {}

newBenchmark("ProxyConstructorWithArrowFunc", {
  setup() { },
  run() {
    var proxy = new Proxy(foo, {});
    result = proxy;
  },
  teardown() {
    return (typeof result == 'function');
  }
});

// ----------------------------------------------------------------------------

class Class {};

newBenchmark("ProxyConstructorWithClass", {
  setup() { },
  run() {
    var proxy = new Proxy(Class, {});
    result = proxy;
  },
  teardown() {
    return (typeof result == 'function');
  }
});

// ----------------------------------------------------------------------------

let obj = {};

newBenchmark("ProxyConstructorWithObject", {
  setup() { },
  run() {
    var proxy = new Proxy(obj, {});
    result = proxy;
  },
  teardown() {
    return (typeof result == 'function');
  }
});

// ----------------------------------------------------------------------------

var p = new Proxy({}, {});

newBenchmark("ProxyConstructorWithProxy", {
  setup() { },
  run() {
    var proxy = new Proxy(p, {});
    result = proxy;
  },
  teardown() {
    return (typeof result == 'function');
  }
});

// ----------------------------------------------------------------------------

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
      p();
    }
  },
  teardown() {
    return (result === SOME_NUMBER);
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
      p();
    }
  },
  teardown() {
    return (result === SOME_OTHER_NUMBER);
  }
});

var instance;
class MyClass {
};

// ----------------------------------------------------------------------------

newBenchmark("ConstructProxyWithoutTrap", {
  setup() {
    p = new Proxy(MyClass, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      instance = new p();
    }
  },
  teardown() {
    return instance instanceof MyClass;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("ConstructProxyWithTrap", {
  setup() {
    p = new Proxy(Object, {
      construct: function(target, argumentsList, newTarget) {
        return new MyClass;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      instance = new p();
    }
  },
  teardown() {
    return instance instanceof MyClass;
  }
});

// ----------------------------------------------------------------------------

obj = {
  prop: SOME_NUMBER
}
let value;

newBenchmark("GetStringWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = p.prop;
    }
  },
  teardown() {
    return value === SOME_NUMBER;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetStringWithTrap", {
  setup() {
    p = new Proxy(obj, {
      get: function(target, propertyKey, receiver) {
        return SOME_OTHER_NUMBER;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = p.prop;
    }
  },
  teardown() {
    return value === SOME_OTHER_NUMBER;
  }
});

// ----------------------------------------------------------------------------

obj = [SOME_NUMBER];

newBenchmark("GetIndexWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = p[0];
    }
  },
  teardown() {
    return value === SOME_NUMBER;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetIndexWithTrap", {
  setup() {
    p = new Proxy(obj, {
      get: function(target, propertyKey, receiver) {
        return SOME_OTHER_NUMBER;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = p[0];
    }
  },
  teardown() {
    return value === SOME_OTHER_NUMBER;
  }
});

// ----------------------------------------------------------------------------

var symbol = Symbol();
obj[symbol] = SOME_NUMBER;

newBenchmark("GetSymbolWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = p[symbol];
    }
  },
  teardown() {
    return value === SOME_NUMBER;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("GetSymbolWithTrap", {
  setup() {
    p = new Proxy(obj, {
      get: function(target, propertyKey, receiver) {
        return SOME_OTHER_NUMBER;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = p[symbol];
    }
  },
  teardown() {
    return value === SOME_OTHER_NUMBER;
  }
});

// ----------------------------------------------------------------------------

obj = {};

newBenchmark("HasStringWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = ('prop' in p);
    }
  },
  teardown() {
    return value === true;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("HasStringWithTrap", {
  setup() {
    p = new Proxy(obj, {
      has: function(target, propertyKey) {
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = ('prop' in p);
    }
  },
  teardown() {
    return value === true;
  }
});

// ----------------------------------------------------------------------------

obj[symbol] = SOME_NUMBER;

newBenchmark("HasSymbolWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = (symbol in p);
    }
  },
  teardown() {
    return value === true;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("HasSymbolWithTrap", {
  setup() {
    p = new Proxy(obj, {
      has: function(target, propertyKey) {
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = (symbol in p);
    }
  },
  teardown() {
    return value === true;
  }
});

// ----------------------------------------------------------------------------

obj = {
  prop: undefined
}
value = SOME_NUMBER;

newBenchmark("SetStringWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p.prop = value;
    }
  },
  teardown() {
    return value === SOME_NUMBER;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetStringWithTrap", {
  setup() {
    p = new Proxy(obj, {
      set: function(target, propertyKey, value, receiver) {
        target[propertyKey] = SOME_OTHER_NUMBER;
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p.prop = value;
    }
  },
  teardown() {
    return value === SOME_OTHER_NUMBER;
  }
});

// ----------------------------------------------------------------------------

obj = [undefined];
value = SOME_NUMBER;

newBenchmark("SetIndexWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p[0] = value;
    }
  },
  teardown() {
    return value === SOME_NUMBER;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetIndexWithTrap", {
  setup() {
    p = new Proxy(obj, {
      set: function(target, propertyKey, value, receiver) {
        target[propertyKey] = SOME_OTHER_NUMBER;
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p[0] = value;
    }
  },
  teardown() {
    return value === SOME_OTHER_NUMBER;
  }
});
// ----------------------------------------------------------------------------

obj[symbol] = undefined;
value = SOME_NUMBER;

newBenchmark("SetSymbolWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p[symbol] = value;
    }
  },
  teardown() {
    return value === SOME_NUMBER;
  }
});

// ----------------------------------------------------------------------------

newBenchmark("SetSymbolWithTrap", {
  setup() {
    p = new Proxy(obj, {
      set: function(target, propertyKey, value, receiver) {
        target[propertyKey] = SOME_OTHER_NUMBER;
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      p[symbol] = value;
    }
  },
  teardown() {
    return value === SOME_OTHER_NUMBER;
  }
});

// ----------------------------------------------------------------------------

var obj20prop = {};
var measured;

newBenchmark("HasInIdiom", {
  setup() {
    for (var i = 0; i < 20; ++i) {
      obj20prop['prop' + i] = SOME_NUMBER;
    }
    p = new Proxy(obj20prop, {
      has: function(target, propertyKey) {
        return true;
      },
      get: function(target, propertyKey, receiver) {
        if (typeof propertyKey == 'string' && propertyKey.match('prop'))
          return SOME_NUMBER;
        else
          return Reflect.get(target, propertyKey, receiver);
      },
    });
    measured = function measured(o) {
      var result = 0;
      for (var x in o) {
        if (Object.prototype.hasOwnProperty(o, x)) {
          var v = o[x];
          result += v;
        }
      }
      return result;
    }
  },
  run() {
    result = measured(p);
  },
  teardown() {
    return result === 20 * SOME_NUMBER;
  }
});

// ----------------------------------------------------------------------------

obj = {};
value = false;

newBenchmark("IsExtensibleWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = Object.isExtensible(p);
    }
    return value;
  },
  teardown() {
    return value === true;
  }
});

// ----------------------------------------------------------------------------

obj = {};
value = false;

newBenchmark("IsExtensibleWithTrap", {
  setup() {
    p = new Proxy(obj, {
      isExtensible: function(target) {
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = Object.isExtensible(p);
    }
    return value;
  },
  teardown() {
    return value === true;
  }
});

// ----------------------------------------------------------------------------

obj = {};
value = false;

newBenchmark("PreventExtensionsWithoutTrap", {
  setup() {
    p = new Proxy(obj, {});
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = Object.preventExtensions(p);
    }
    return value;
  },
  teardown() {}
});

// ----------------------------------------------------------------------------

obj = {};
value = false;

newBenchmark("PreventExtensionsWithTrap", {
  setup() {
    p = new Proxy(obj, {
      preventExtensions: function(target) {
        Object.preventExtensions(target);
        return true;
      }
    });
  },
  run() {
    for(var i = 0; i < ITERATIONS; i++) {
      value = Object.preventExtensions(p);
    }
    return value;
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
      value = Object.getPrototypeOf(p);
    }
    return value;
  },
  teardown() {}
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
      value = Object.getPrototypeOf(p);
    }
    return value;
  },
  teardown() {}
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
      value = Object.setPrototypeOf(p, [1]);
    }
    return value;
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
      value = Object.setPrototypeOf(p, [1]);
    }
    return value;
  },
  teardown() {}
});
