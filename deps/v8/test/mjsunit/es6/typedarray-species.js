// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Subclasses of %TypedArray% construct themselves under map, etc

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array
];

for (let constructor of typedArrayConstructors) {
  class MyTypedArray extends constructor { }
  assertEquals(MyTypedArray, new MyTypedArray().map(()=>0).constructor);
  assertEquals(MyTypedArray, new MyTypedArray().filter(()=>{}).constructor);
  assertEquals(MyTypedArray, new MyTypedArray().slice().constructor);
}

// Subclasses can override @@species to return the another class

for (let constructor of typedArrayConstructors) {
  class MyTypedArray extends constructor { }
  class MyOtherTypedArray extends constructor {
    static get [Symbol.species]() { return MyTypedArray; }
  }
  assertEquals(MyTypedArray, new MyOtherTypedArray().map(()=>0).constructor);
  assertEquals(MyTypedArray, new MyOtherTypedArray().filter(()=>{}).constructor);
  assertEquals(MyTypedArray, new MyOtherTypedArray().slice().constructor);
}

// TypedArray too-short and non-TypedArray error checking

for (let constructor of typedArrayConstructors) {
  class MyShortTypedArray extends constructor {
    constructor(length) { super(length - 1); }
  }
  assertThrows(() => new MyShortTypedArray(5).map(()=>0), TypeError);
  assertThrows(() => new MyShortTypedArray(5).filter(()=>true), TypeError);
  assertThrows(() => new MyShortTypedArray(5).slice(), TypeError);

  class MyNonTypedArray extends constructor {
    static get [Symbol.species]() { return Array; }
  }
  assertThrows(() => new MyNonTypedArray().map(()=>0), TypeError);
  assertThrows(() => new MyNonTypedArray().filter(()=>{}), TypeError);
  assertThrows(() => new MyNonTypedArray().slice(), TypeError);
}

// Defaults when constructor or @@species is missing or non-constructor

for (let constructor of typedArrayConstructors) {
  class MyDefaultTypedArray extends constructor {
    static get [Symbol.species]() { return undefined; }
  }
  assertEquals(constructor, new MyDefaultTypedArray().map(()=>0).constructor);

  class MyOtherDefaultTypedArray extends constructor { }
  assertEquals(MyOtherDefaultTypedArray, new MyOtherDefaultTypedArray().map(()=>0).constructor);
  MyOtherDefaultTypedArray.prototype.constructor = undefined;
  assertEquals(constructor, new MyOtherDefaultTypedArray().map(()=>0).constructor);
}

// Exceptions propagated when getting constructor @@species throws

class SpeciesError extends Error { }
class ConstructorError extends Error { }

for (let constructor of typedArrayConstructors) {
  class MyThrowingArray extends constructor {
    static get [Symbol.species]() { throw new SpeciesError; }
  }
  assertThrows(() => new MyThrowingArray().map(()=>{}), SpeciesError);
  Object.defineProperty(MyThrowingArray.prototype, 'constructor', {
      get() { throw new ConstructorError; }
  });
  assertThrows(() => new MyThrowingArray().map(()=>{}), ConstructorError);
}
