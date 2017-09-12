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

var obj = {};

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
