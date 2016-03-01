// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noharmony-species

// Before Symbol.species was added, ArrayBuffer subclasses constructed
// ArrayBuffers, and Array subclasses constructed Arrays, but TypedArray and
// Promise subclasses constructed an instance of the subclass.

'use strict';

assertEquals(undefined, Symbol.species);

class MyArray extends Array { }
let myArray = new MyArray();
assertEquals(MyArray, myArray.constructor);
assertEquals(Array, myArray.map(x => x + 1).constructor);
assertEquals(Array, myArray.concat().constructor);

class MyUint8Array extends Uint8Array { }
Object.defineProperty(MyUint8Array.prototype, "BYTES_PER_ELEMENT", {value: 1});
let myTypedArray = new MyUint8Array(3);
assertEquals(MyUint8Array, myTypedArray.constructor);
assertEquals(MyUint8Array, myTypedArray.map(x => x + 1).constructor);

class MyArrayBuffer extends ArrayBuffer { }
let myBuffer = new MyArrayBuffer(0);
assertEquals(MyArrayBuffer, myBuffer.constructor);
assertEquals(ArrayBuffer, myBuffer.slice().constructor);

class MyPromise extends Promise { }
let myPromise = new MyPromise(() => {});
assertEquals(MyPromise, myPromise.constructor);
assertEquals(MyPromise, myPromise.then().constructor);

// However, subarray instantiates members of the parent class
assertEquals(Uint8Array, myTypedArray.subarray(1).constructor);
