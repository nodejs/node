// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = this;
var globalProto = Object.getPrototypeOf(global);

// Number of objects being tested. There is an assert ensuring this is correct.
var objectCount = 21;


function runTest(f) {
  function restore(object, oldProto) {
    delete object[Symbol.unscopables];
    delete object.x;
    delete object.x_;
    delete object.y;
    delete object.z;
    Object.setPrototypeOf(object, oldProto);
  }

  function getObject(i) {
    var objects = [
      {},
      [],
      function() {},
      function() {
        return arguments;
      }(),
      function() {
        'use strict';
        return arguments;
      }(),
      Object(1),
      Object(true),
      Object('bla'),
      new Date,
      new RegExp,
      new Set,
      new Map,
      new WeakMap,
      new WeakSet,
      new ArrayBuffer(10),
      new Int32Array(5),
      Object,
      Function,
      Date,
      RegExp,
      global
    ];

    assertEquals(objectCount, objects.length);
    return objects[i];
  }

  // Tests depends on this not being there to start with.
  delete Array.prototype[Symbol.unscopables];

  if (f.length === 1) {
    for (var i = 0; i < objectCount; i++) {
      var object = getObject(i);
      var oldObjectProto = Object.getPrototypeOf(object);
      f(object);
      restore(object, oldObjectProto);
    }
  } else {
    for (var i = 0; i < objectCount; i++) {
      for (var j = 0; j < objectCount; j++) {
        var object = getObject(i);
        var proto = getObject(j);
        if (object === proto) {
          continue;
        }
        var oldObjectProto = Object.getPrototypeOf(object);
        var oldProtoProto = Object.getPrototypeOf(proto);
        f(object, proto);
        restore(object, oldObjectProto);
        restore(proto, oldProtoProto);
      }
    }
  }
}

// Test array first, since other tests are changing
// Array.prototype[Symbol.unscopables].
function TestArrayPrototypeUnscopables() {
  var descr = Object.getOwnPropertyDescriptor(Array.prototype,
                                              Symbol.unscopables);
  assertFalse(descr.enumerable);
  assertFalse(descr.writable);
  assertTrue(descr.configurable);
  assertEquals(null, Object.getPrototypeOf(descr.value));

  var copyWithin = 'local copyWithin';
  var entries = 'local entries';
  var fill = 'local fill';
  var find = 'local find';
  var findIndex = 'local findIndex';
  var flat = 'local flat';
  var flatMap = 'local flatMap';
  var keys = 'local keys';
  var includes = 'local includes';
  var values = 'local values';

  var array = [];
  array.toString = 42;

  with (array) {
    assertEquals('local copyWithin', copyWithin);
    assertEquals('local entries', entries);
    assertEquals('local fill', fill);
    assertEquals('local find', find);
    assertEquals('local findIndex', findIndex);
    assertEquals('local flat', flat);
    assertEquals('local flatMap', flatMap);
    assertEquals('local includes', includes);
    assertEquals('local keys', keys);
    assertEquals('local values', values);
    assertEquals(42, toString);
  }
}
TestArrayPrototypeUnscopables();



function TestBasics(object) {
  var x = 1;
  var y = 2;
  var z = 3;
  object.x = 4;
  object.y = 5;

  with (object) {
    assertEquals(4, x);
    assertEquals(5, y);
    assertEquals(3, z);
  }

  var truthyValues = [true, 1, 'x', {}, Symbol()];
  for (var truthyValue of truthyValues) {
    object[Symbol.unscopables] = {x: truthyValue};
    with (object) {
      assertEquals(1, x);
      assertEquals(5, y);
      assertEquals(3, z);
    }
  }

  var falsyValues = [false, 0, -0, NaN, '', null, undefined];
  for (var falsyValue of falsyValues) {
    object[Symbol.unscopables] = {x: falsyValue, y: true};
    with (object) {
      assertEquals(4, x);
      assertEquals(2, y);
      assertEquals(3, z);
    }
  }

  for (var xFalsy of falsyValues) {
    for (var yFalsy of falsyValues) {
      object[Symbol.unscopables] = {x: xFalsy, y: yFalsy};
      with (object) {
        assertEquals(4, x);
        assertEquals(5, y);
        assertEquals(3, z);
      }
    }
  }
}
runTest(TestBasics);


function TestUnscopableChain(object) {
  var x = 1;
  object.x = 2;

  with (object) {
    assertEquals(2, x);
  }

  object[Symbol.unscopables] = {
    __proto__: {x: true}
  };
  with (object) {
    assertEquals(1, x);
  }

  object[Symbol.unscopables] = {
    __proto__: {x: undefined}
  };
  with (object) {
    assertEquals(2, x);
  }
}
runTest(TestUnscopableChain);


function TestBasicsSet(object) {
  var x = 1;
  object.x = 2;

  with (object) {
    assertEquals(2, x);
  }

  object[Symbol.unscopables] = {x: true};
  with (object) {
    assertEquals(1, x);
    x = 3;
    assertEquals(3, x);
  }

  assertEquals(3, x);
  assertEquals(2, object.x);
}
runTest(TestBasicsSet);


function TestOnProto(object, proto) {
  var x = 1;
  var y = 2;
  var z = 3;
  proto.x = 4;

  Object.setPrototypeOf(object, proto);
  object.y = 5;

  with (object) {
    assertEquals(4, x);
    assertEquals(5, y);
    assertEquals(3, z);
  }

  proto[Symbol.unscopables] = {x: true};
  with (object) {
    assertEquals(1, x);
    assertEquals(5, y);
    assertEquals(3, z);
  }

  object[Symbol.unscopables] = {y: true};
  with (object) {
    assertEquals(4, x);
    assertEquals(2, y);
    assertEquals(3, z);
  }

  proto[Symbol.unscopables] = {y: true};
  object[Symbol.unscopables] = {x: true};
  with (object) {
    assertEquals(1, x);
    assertEquals(5, y);
    assertEquals(3, z);
  }

  proto[Symbol.unscopables] = {y: true};
  object[Symbol.unscopables] = {x: true, y: undefined};
  with (object) {
    assertEquals(1, x);
    assertEquals(5, y);
    assertEquals(3, z);
  }
}
runTest(TestOnProto);


function TestSetBlockedOnProto(object, proto) {
  var x = 1;
  object.x = 2;

  with (object) {
    assertEquals(2, x);
  }

  Object.setPrototypeOf(object, proto);
  proto[Symbol.unscopables] = {x: true};
  with (object) {
    assertEquals(1, x);
    x = 3;
    assertEquals(3, x);
  }

  assertEquals(3, x);
  assertEquals(2, object.x);
}
runTest(TestSetBlockedOnProto);


function TestNonObject(object) {
  var x = 1;
  var y = 2;
  object.x = 3;
  object.y = 4;

  object[Symbol.unscopables] = 'xy';
  with (object) {
    assertEquals(3, x);
    assertEquals(4, y);
  }

  object[Symbol.unscopables] = null;
  with (object) {
    assertEquals(3, x);
    assertEquals(4, y);
  }
}
runTest(TestNonObject);


function TestChangeDuringWith(object) {
  var x = 1;
  var y = 2;
  object.x = 3;
  object.y = 4;

  with (object) {
    assertEquals(3, x);
    assertEquals(4, y);
    object[Symbol.unscopables] = {x: true};
    assertEquals(1, x);
    assertEquals(4, y);
  }
}
runTest(TestChangeDuringWith);


function TestChangeDuringWithWithPossibleOptimization(object) {
  var x = 1;
  object.x = 2;
  with (object) {
    for (var i = 0; i < 1000; i++) {
      if (i === 500) object[Symbol.unscopables] = {x: true};
      assertEquals(i < 500 ? 2: 1, x);
    }
  }
}
TestChangeDuringWithWithPossibleOptimization({});


function TestChangeDuringWithWithPossibleOptimization2(object) {
  var x = 1;
  object.x = 2;
  object[Symbol.unscopables] = {x: true};
  with (object) {
    for (var i = 0; i < 1000; i++) {
      if (i === 500) delete object[Symbol.unscopables];
      assertEquals(i < 500 ? 1 : 2, x);
    }
  }
}
TestChangeDuringWithWithPossibleOptimization2({});


function TestChangeDuringWithWithPossibleOptimization3(object) {
  var x = 1;
  object.x = 2;
  object[Symbol.unscopables] = {};
  with (object) {
    for (var i = 0; i < 1000; i++) {
      if (i === 500) object[Symbol.unscopables].x = true;
      assertEquals(i < 500 ? 2 : 1, x);
    }
  }
}
TestChangeDuringWithWithPossibleOptimization3({});


function TestChangeDuringWithWithPossibleOptimization4(object) {
  var x = 1;
  object.x = 2;
  object[Symbol.unscopables] = {x: true};
  with (object) {
    for (var i = 0; i < 1000; i++) {
      if (i === 500) delete object[Symbol.unscopables].x;
      assertEquals(i < 500 ? 1 : 2, x);
    }
  }
}
TestChangeDuringWithWithPossibleOptimization4({});


function TestChangeDuringWithWithPossibleOptimization4(object) {
  var x = 1;
  object.x = 2;
  object[Symbol.unscopables] = {x: true};
  with (object) {
    for (var i = 0; i < 1000; i++) {
      if (i === 500) object[Symbol.unscopables].x = undefined;
      assertEquals(i < 500 ? 1 : 2, x);
    }
  }
}
TestChangeDuringWithWithPossibleOptimization4({});


function TestAccessorReceiver(object, proto) {
  var x = 'local';

  Object.defineProperty(proto, 'x', {
    get: function() {
      assertEquals(object, this);
      return this.x_;
    },
    configurable: true
  });
  proto.x_ = 'proto';

  Object.setPrototypeOf(object, proto);
  proto.x_ = 'object';

  with (object) {
    assertEquals('object', x);
  }
}
runTest(TestAccessorReceiver);


function TestUnscopablesGetter(object) {
  // This test gets really messy when object is the global since the assert
  // functions are properties on the global object and the call count gets
  // completely different.
  if (object === global) return;

  var x = 'local';
  object.x = 'object';

  var callCount = 0;
  Object.defineProperty(object, Symbol.unscopables, {
    get: function() {
      callCount++;
      return {};
    },
    configurable: true
  });
  with (object) {
    assertEquals('object', x);
  }
  // Once for HasBinding
  assertEquals(1, callCount);

  callCount = 0;
  Object.defineProperty(object, Symbol.unscopables, {
    get: function() {
      callCount++;
      return {x: true};
    },
    configurable: true
  });
  with (object) {
    assertEquals('local', x);
  }
  // Once for HasBinding
  assertEquals(1, callCount);

  callCount = 0;
  Object.defineProperty(object, Symbol.unscopables, {
    get: function() {
      callCount++;
      return callCount == 1 ? {} : {x: true};
    },
    configurable: true
  });
  with (object) {
    x = 1;
  }
  // Once for HasBinding
  assertEquals(1, callCount);
  assertEquals(1, object.x);
  assertEquals('local', x);
  with (object) {
    x = 2;
  }
  // One more HasBinding.
  assertEquals(2, callCount);
  assertEquals(1, object.x);
  assertEquals(2, x);
}
runTest(TestUnscopablesGetter);


var global = this;
function TestUnscopablesGetter2() {
  var x = 'local';

  var globalProto = Object.getPrototypeOf(global);
  var protos = [{}, [], function() {}, global];
  var objects = [{}, [], function() {}];

  protos.forEach(function(proto) {
    objects.forEach(function(object) {
      Object.defineProperty(proto, 'x', {
        get: function() {
          assertEquals(object, this);
          return 'proto';
        },
        configurable: true
      });

      object.__proto__ = proto;
      Object.defineProperty(object, 'x', {
        get: function() {
          assertEquals(object, this);
          return 'object';
        },
        configurable: true
      });

      with (object) {
        assertEquals('object', x);
      }

      object[Symbol.unscopables] = {x: true};
      with (object) {
        assertEquals('local', x);
      }

      delete proto[Symbol.unscopables];
      delete object[Symbol.unscopables];
    });
  });

  delete global.x;
  Object.setPrototypeOf(global, globalProto);
}
TestUnscopablesGetter2();


function TestSetterOnBlocklisted(object, proto) {
  var x = 'local';
  Object.defineProperty(proto, 'x', {
    set: function(x) {
      assertUnreachable();
    },
    get: function() {
      return 'proto';
    },
    configurable: true
  });
  Object.setPrototypeOf(object, proto);
  Object.defineProperty(object, 'x', {
    get: function() {
      return this.x_;
    },
    set: function(x) {
      this.x_ = x;
    },
    configurable: true
  });
  object.x_ = 1;

  with (object) {
    x = 2;
    assertEquals(2, x);
  }

  assertEquals(2, object.x);

  object[Symbol.unscopables] = {x: true};

  with (object) {
    x = 3;
    assertEquals(3, x);
  }

  assertEquals(2, object.x);
}
runTest(TestSetterOnBlocklisted);


function TestObjectsAsUnscopables(object, unscopables) {
  var x = 1;
  object.x = 2;

  with (object) {
    assertEquals(2, x);
    object[Symbol.unscopables] = unscopables;
    assertEquals(2, x);
  }
}
runTest(TestObjectsAsUnscopables);


function TestAccessorOnUnscopables(object) {
  var x = 1;
  object.x = 2;

  var calls = 0;
  var unscopables = {
    get x() {
      calls++;
      return calls === 1 ? true : undefined;
    }
  };

  with (object) {
    assertEquals(2, x);
    object[Symbol.unscopables] = unscopables;
    assertEquals(1, x);
    assertEquals(2, x);
  }
  assertEquals(2, calls);
}
runTest(TestAccessorOnUnscopables);


function TestLengthUnscopables(object, proto) {
  var length = 2;
  with (object) {
    assertEquals(1, length);
    object[Symbol.unscopables] = {length: true};
    assertEquals(2, length);
    delete object[Symbol.unscopables];
    assertEquals(1, length);
  }
}
TestLengthUnscopables([1], Array.prototype);
TestLengthUnscopables(function(x) {}, Function.prototype);
TestLengthUnscopables(new String('x'), String.prototype);


function TestFunctionNameUnscopables(object) {
  var name = 'local';
  with (object) {
    assertEquals('f', name);
    object[Symbol.unscopables] = {name: true};
    assertEquals('local', name);
    delete object[Symbol.unscopables];
    assertEquals('f', name);
  }
}
TestFunctionNameUnscopables(function f() {});


function TestFunctionPrototypeUnscopables() {
  var prototype = 'local';
  var f = function() {};
  var g = function() {};
  Object.setPrototypeOf(f, g);
  var fp = f.prototype;
  var gp = g.prototype;
  with (f) {
    assertEquals(fp, prototype);
    f[Symbol.unscopables] = {prototype: true};
    assertEquals('local', prototype);
    delete f[Symbol.unscopables];
    assertEquals(fp, prototype);
  }
}
TestFunctionPrototypeUnscopables(function() {});


function TestFunctionArgumentsUnscopables() {
  var func = function() {
    var arguments = 'local';
    var args = func.arguments;
    with (func) {
      assertEquals(args, arguments);
      func[Symbol.unscopables] = {arguments: true};
      assertEquals('local', arguments);
      delete func[Symbol.unscopables];
      assertEquals(args, arguments);
    }
  }
  func(1);
}
TestFunctionArgumentsUnscopables();


function TestArgumentsLengthUnscopables() {
  var func = function() {
    var length = 'local';
    with (arguments) {
      assertEquals(1, length);
      arguments[Symbol.unscopables] = {length: true};
      assertEquals('local', length);
    }
  }
  func(1);
}
TestArgumentsLengthUnscopables();


function TestFunctionCallerUnscopables() {
  var func = function() {
    var caller = 'local';
    with (func) {
      assertEquals(TestFunctionCallerUnscopables, caller);
      func[Symbol.unscopables] = {caller: true};
      assertEquals('local', caller);
      delete func[Symbol.unscopables];
      assertEquals(TestFunctionCallerUnscopables, caller);
    }
  }
  func(1);
}
TestFunctionCallerUnscopables();


function TestGetUnscopablesGetterThrows() {
  var object = {
    get x() {
      assertUnreachable();
    }
  };
  function CustomError() {}
  Object.defineProperty(object, Symbol.unscopables, {
    get: function() {
      throw new CustomError();
    }
  });
  assertThrows(function() {
    with (object) {
      x;
    }
  }, CustomError);
}
TestGetUnscopablesGetterThrows();


function TestGetUnscopablesGetterThrows2() {
  var object = {
    get x() {
      assertUnreachable();
    }
  };
  function CustomError() {}

  object[Symbol.unscopables] = {
    get x() {
      throw new CustomError();
    }
  };
  assertThrows(function() {
    with (object) {
      x;
    }
  }, CustomError);
}
TestGetUnscopablesGetterThrows();
