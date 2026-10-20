// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function newBenchmark(name, handlers) {
  createSuite(name, 1000, handlers.run, handlers.setup, handlers.teardown);
}

const OBJECT_COUNT = 0x80;
const ITERATIONS = 1_000;
const EXPECTED_RESULT = 124650;

let testAccessors;
let result;
let objects;


function createOwnMonomorphic(count) {
  let val = 0;
  let descriptor = {
    get: function () {
      return val;
    },
    set: function (v) {
      val = v;
    },
    enumerable: true,
    configurable: true,
  };
  const objs = [];
  for (let i = 0; i < count; i++) {
    val = i;
    let obj = { variant: "monomorphic accessors" };
    Object.defineProperty(obj, "p", descriptor);
    objs.push(obj);
  }
  return objs;
}

function createOwnPolymorphic(count) {
  const objs = [];
  for (let i = 0; i < count; i++) {
    let val = i;
    let obj = {
      variant: "polymorphic accessors",
      get p() {
        return val;
      },
      set p(v) {
        val = v;
      },
    };
    objs.push(obj);
  }
  return objs;
}

function createProtoObjects(count) {
  let val;
  let descriptor = {
    get: function () {
      return val;
    },
    set: function (v) {
      val = v;
    },
    enumerable: true,
    configurable: true,
  };
  const proto = {};
  Object.defineProperty(proto, "p", descriptor);

  // Turn it into prototype mode and ensure it's switched from setup to
  // fast mode.
  let o = Object.create(proto);
  for (let i = 0; i < 100; i++) {
    o.p;
  }

  const objs = [];
  for (let i = 0; i < count; i++) {
    val = i;
    let obj = {
      variant: "prototype chain accessors",
      __proto__: proto,
    };
    objs.push(obj);
  }

  return objs;
}


let unique_id = 0;

// Creates unique tester function instance having the same body, so that the
// feedback vector is not shared across test variants.
function createTester() {
  function testAccessors(obj, v) {
    let value = obj.p;
    obj.p = v;
    return value;
  }
  var f = eval(`/* ${unique_id} */ (${testAccessors.toString()})`);
  unique_id++;
  return f;
}

// ----------------------------------------------------------------------------

newBenchmark("Monomorphic", {
  setup() {
    testAccessors = createTester();
    objects = createOwnMonomorphic(OBJECT_COUNT);
  },
  run() {
    result = 0;
    for (var i = 0; i < ITERATIONS; i++) {
      result += testAccessors(objects[i % OBJECT_COUNT], i % 0xff);
    }
  },
  teardown() {
    assert(result === EXPECTED_RESULT, `wrong result: ${result}`);
  },
});


// ----------------------------------------------------------------------------

newBenchmark("Polymorphic", {
  setup() {
    testAccessors = createTester();
    objects = createOwnPolymorphic(OBJECT_COUNT);
  },
  run() {
    result = 0;
    for (var i = 0; i < ITERATIONS; i++) {
      result += testAccessors(objects[i % OBJECT_COUNT], i % 0xff);
    }
  },
  teardown() {
    assert(result === EXPECTED_RESULT, `wrong result: ${result}`);
  },
});

// ----------------------------------------------------------------------------

newBenchmark("PrototypeConst", {
  setup() {
    testAccessors = createTester();
    objects = createProtoObjects(OBJECT_COUNT);
  },
  run() {
    result = 0;
    for (var i = 0; i < ITERATIONS; i++) {
      result += testAccessors(objects[i % OBJECT_COUNT], i % 0xff);
    }
  },
  teardown() {
    assert(result === EXPECTED_RESULT, `wrong result: ${result}`);
  },
});
