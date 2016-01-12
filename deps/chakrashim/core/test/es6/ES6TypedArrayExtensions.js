//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 TypedArray extension tests -- verifies the API shape and basic functionality

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Uint8ClampedArray constructor exists",
        body: function () {
            assert.isTrue(typeof Uint8ClampedArray === 'function', "Uint8ClampedArray constructor exists");
        }
    },
    {
        name: "%TypedArray% intrinsic has correct properties",
        body: function() {
            function verifyTypedArrayConstructorPropertyValue(constructor, name, type, configurableIsTrue) {
                var descriptor = Object.getOwnPropertyDescriptor(constructor, name);
                var propName = constructor.name + "." + name;

                assert.isFalse(descriptor.writable, propName + ".writable === false");
                assert.isFalse(descriptor.enumerable, propName + ".enumerable === false");
                assert.areEqual(configurableIsTrue, descriptor.configurable, propName + ".configurable === " +configurableIsTrue);
                assert.areEqual(type, typeof descriptor.value, "typeof " + propName + " === '" + type + "'");
            }

            var typedArrayConstructor = Int8Array.__proto__;

            assert.areEqual('function', typeof typedArrayConstructor, "typeof %TypedArray% === 'function'");

            assert.areEqual('TypedArray', typedArrayConstructor.name, "%TypedArray%.name === 'TypedArray'");
            assert.areEqual(3, typedArrayConstructor.length, "%TypedArray%.length === 3");

            assert.isTrue(typedArrayConstructor === Uint8Array.__proto__, "All TypedArray constructors have their [[prototype]] slot set to the %TypedArray% intrinsic");
            assert.isTrue(typedArrayConstructor === Uint8ClampedArray.__proto__, "All TypedArray constructors have their [[prototype]] slot set to the %TypedArray% intrinsic");
            assert.isTrue(typedArrayConstructor === Int16Array.__proto__, "All TypedArray constructors have their [[prototype]] slot set to the %TypedArray% intrinsic");
            assert.isTrue(typedArrayConstructor === Uint16Array.__proto__, "All TypedArray constructors have their [[prototype]] slot set to the %TypedArray% intrinsic");
            assert.isTrue(typedArrayConstructor === Int32Array.__proto__, "All TypedArray constructors have their [[prototype]] slot set to the %TypedArray% intrinsic");
            assert.isTrue(typedArrayConstructor === Uint32Array.__proto__, "All TypedArray constructors have their [[prototype]] slot set to the %TypedArray% intrinsic");
            assert.isTrue(typedArrayConstructor === Float32Array.__proto__, "All TypedArray constructors have their [[prototype]] slot set to the %TypedArray% intrinsic");
            assert.isTrue(typedArrayConstructor === Float64Array.__proto__, "All TypedArray constructors have their [[prototype]] slot set to the %TypedArray% intrinsic");

            verifyTypedArrayConstructorPropertyValue(typedArrayConstructor, 'length', 'number',false);
            verifyTypedArrayConstructorPropertyValue(typedArrayConstructor, 'name', 'string',true);

            assert.isFalse(typedArrayConstructor.from === undefined, "%TypedArray%.from !== undefined");
            assert.areEqual('function', typeof typedArrayConstructor.from, "typeof %TypedArray%.from == 'function'");
            assert.areEqual(1, typedArrayConstructor.from.length, "%TypedArray%.from.length == 1");

            var descriptor = Object.getOwnPropertyDescriptor(typedArrayConstructor, 'from');

            assert.isTrue(descriptor.writable, "%TypedArray%.from.writable === true");
            assert.isFalse(descriptor.enumerable, "%TypedArray%.from.enumerable === false");
            assert.isTrue(descriptor.configurable, "%TypedArray%.from.configurable === true");

            assert.isFalse(typedArrayConstructor.of === undefined, "%TypedArray%.of !== undefined");
            assert.areEqual('function', typeof typedArrayConstructor.of, "typeof %TypedArray%.of == 'function'");
            assert.areEqual(0, typedArrayConstructor.of.length, "%TypedArray%.of.length == 0");

            var descriptor = Object.getOwnPropertyDescriptor(typedArrayConstructor, 'of');

            assert.isTrue(descriptor.writable, "%TypedArray%.of.writable === true");
            assert.isFalse(descriptor.enumerable, "%TypedArray%.of.enumerable === false");
            assert.isTrue(descriptor.configurable, "%TypedArray%.of.configurable === true");

        }
    },
    {
        name: "%TypedArrayPrototype% intrinsic has correct properties",
        body: function () {
            function verifyTypedArrayPrototypePropertyAccessor(obj, name, displayName, functionName) {
                functionName = functionName || name;
                var descriptor = Object.getOwnPropertyDescriptor(obj, name);
                assert.isFalse(descriptor.enumerable, displayName + ".enumerable === false");
                assert.isTrue(descriptor.configurable, displayName + ".configurable === true");
                assert.areEqual('function', typeof descriptor.get, "typeof " + displayName + ".get === 'function'");
                assert.areEqual(undefined, descriptor.set, displayName + ".set === undefined");
                assert.areEqual(0, descriptor.get.length, displayName + ".get.length === 0");

                var actualName = descriptor.get.toString();
                assert.areEqual(functionName, actualName, displayName + ".get.name = " + functionName);
            }
            function verifyTypedArrayPrototypePropertyFunction(obj, name, len, displayName) {
                var descriptor = Object.getOwnPropertyDescriptor(obj, name);
                assert.isTrue(descriptor.writable, displayName + ".writable === true");
                assert.isFalse(descriptor.enumerable, displayName + ".enumerable === false");
                assert.isTrue(descriptor.configurable, displayName + ".configurable === true");
                assert.areEqual('function', typeof descriptor.value, "typeof " + displayName + " === 'function'");
                assert.areEqual(len, descriptor.value.length, displayName + ".length === " + len);
            }

            var typedArrayPrototype = Int8Array.prototype.__proto__;

            assert.areEqual('object', typeof typedArrayPrototype, "typeof %TypedArrayPrototype% === 'object'");
            assert.areEqual(Int8Array.__proto__, typedArrayPrototype.constructor, "%TypedArrayPrototype%.constructor === %TypedArray%");

            assert.isTrue(typedArrayPrototype === Uint8Array.prototype.__proto__, "All TypedArray prototypes have their [[prototype]] slot set to the %TypedArrayPrototype% intrinsic");
            assert.isTrue(typedArrayPrototype === Uint8ClampedArray.prototype.__proto__, "All TypedArray prototypes have their [[prototype]] slot set to the %TypedArrayPrototype% intrinsic");
            assert.isTrue(typedArrayPrototype === Int16Array.prototype.__proto__, "All TypedArray prototypes have their [[prototype]] slot set to the %TypedArrayPrototype% intrinsic");
            assert.isTrue(typedArrayPrototype === Uint16Array.prototype.__proto__, "All TypedArray prototypes have their [[prototype]] slot set to the %TypedArrayPrototype% intrinsic");
            assert.isTrue(typedArrayPrototype === Int32Array.prototype.__proto__, "All TypedArray prototypes have their [[prototype]] slot set to the %TypedArrayPrototype% intrinsic");
            assert.isTrue(typedArrayPrototype === Uint32Array.prototype.__proto__, "All TypedArray prototypes have their [[prototype]] slot set to the %TypedArrayPrototype% intrinsic");
            assert.isTrue(typedArrayPrototype === Float32Array.prototype.__proto__, "All TypedArray prototypes have their [[prototype]] slot set to the %TypedArrayPrototype% intrinsic");
            assert.isTrue(typedArrayPrototype === Float64Array.prototype.__proto__, "All TypedArray prototypes have their [[prototype]] slot set to the %TypedArrayPrototype% intrinsic");

            verifyTypedArrayPrototypePropertyAccessor(typedArrayPrototype, "buffer", "%TypedArrayPrototype%.buffer", "get buffer");
            verifyTypedArrayPrototypePropertyAccessor(typedArrayPrototype, "byteLength", "%TypedArrayPrototype%.byteLength", "get byteLength");
            verifyTypedArrayPrototypePropertyAccessor(typedArrayPrototype, "byteOffset", "%TypedArrayPrototype%.byteOffset", "get byteOffset");
            verifyTypedArrayPrototypePropertyAccessor(typedArrayPrototype, "length", "%TypedArrayPrototype%.length", "get length");
            verifyTypedArrayPrototypePropertyAccessor(typedArrayPrototype, Symbol.toStringTag, "%TypedArrayPrototype%[@@toStringTag]", "get [Symbol.toStringTag]");

            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "set", 2, "%TypedArrayPrototype%.set");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "subarray", 2, "%TypedArrayPrototype%.subarray");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "copyWithin", 2, "%TypedArrayPrototype%.copyWithin");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "fill", 1, "%TypedArrayPrototype%.fill");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "map", 1, "%TypedArrayPrototype%.map");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "forEach", 1, "%TypedArrayPrototype%.forEach");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "indexOf", 1, "%TypedArrayPrototype%.indexOf");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "includes", 1, "%TypedArrayPrototype%.includes");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "every", 1, "%TypedArrayPrototype%.every");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "filter", 1, "%TypedArrayPrototype%.filter");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "find", 1, "%TypedArrayPrototype%.find");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "findIndex", 1, "%TypedArrayPrototype%.findIndex");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "join", 1, "%TypedArrayPrototype%.join");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "lastIndexOf", 1, "%TypedArrayPrototype%.lastIndexOf");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "reduce", 1, "%TypedArrayPrototype%.reduce");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "reduceRight", 1, "%TypedArrayPrototype%.reduceRight");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "reverse", 0, "%TypedArrayPrototype%.reverse");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "slice", 2, "%TypedArrayPrototype%.slice");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "some", 1, "%TypedArrayPrototype%.some");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, "sort", 1, "%TypedArrayPrototype%.sort");

            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, 'entries', 0, "%TypedArrayPrototype%.entries");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, 'keys', 0, "%TypedArrayPrototype%.keys");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, 'values', 0, "%TypedArrayPrototype%.values");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, Symbol.iterator, 0, "%TypedArrayPrototype%[@@iterator]");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, 'toString', 0, "%TypedArrayPrototype%.toString");
            verifyTypedArrayPrototypePropertyFunction(typedArrayPrototype, 'toLocaleString', 0, "%TypedArrayPrototype%.toLocaleString");
        }
    },
    {
        name: "TypedArray constructors have correct properties",
        body: function () {
            function verifyTypedArrayConstructorPropertyValue(constructor, name, type, configurableIsTrue) {
                var descriptor = Object.getOwnPropertyDescriptor(constructor, name);
                var propName = constructor.name + "." + name;

                assert.isFalse(descriptor.writable, propName + ".writable === false");
                assert.isFalse(descriptor.enumerable, propName + ".enumerable === false");
                assert.areEqual(configurableIsTrue, descriptor.configurable, propName + ".configurable === " + configurableIsTrue);
                assert.areEqual(type, typeof descriptor.value, "typeof " + propName + " === '" + type + "'");
            }

            assert.areEqual('Int8Array', Int8Array.name, "Int8Array.name === 'Int8Array'");
            assert.areEqual('Uint8Array', Uint8Array.name, "Uint8Array.name === 'Uint8Array'");
            assert.areEqual('Uint8ClampedArray', Uint8ClampedArray.name, "Uint8ClampedArray.name === 'Uint8ClampedArray'");
            assert.areEqual('Int16Array', Int16Array.name, "Int16Array.name === 'Int16Array'");
            assert.areEqual('Uint16Array', Uint16Array.name, "Uint16Array.name === 'Uint16Array'");
            assert.areEqual('Int32Array', Int32Array.name, "Int32Array.name === 'Int32Array'");
            assert.areEqual('Uint32Array', Uint32Array.name, "Uint32Array.name === 'Uint32Array'");
            assert.areEqual('Float32Array', Float32Array.name, "Float32Array.name === 'Float32Array'");
            assert.areEqual('Float64Array', Float64Array.name, "Float64Array.name === 'Float64Array'");

            verifyTypedArrayConstructorPropertyValue(Int8Array, "name", "string", true);
            verifyTypedArrayConstructorPropertyValue(Uint8Array, "name", "string", true);
            verifyTypedArrayConstructorPropertyValue(Uint8ClampedArray, "name", "string", true);
            verifyTypedArrayConstructorPropertyValue(Int16Array, "name", "string", true);
            verifyTypedArrayConstructorPropertyValue(Uint16Array, "name", "string", true);
            verifyTypedArrayConstructorPropertyValue(Int32Array, "name", "string", true);
            verifyTypedArrayConstructorPropertyValue(Uint32Array, "name", "string", true);
            verifyTypedArrayConstructorPropertyValue(Float32Array, "name", "string", true);
            verifyTypedArrayConstructorPropertyValue(Float64Array, "name", "string", true);

            assert.areEqual(1, Int8Array.BYTES_PER_ELEMENT, "Int8Array.BYTES_PER_ELEMENT === 1");
            assert.areEqual(1, Uint8Array.BYTES_PER_ELEMENT, "Uint8Array.BYTES_PER_ELEMENT === 1");
            assert.areEqual(1, Uint8ClampedArray.BYTES_PER_ELEMENT, "Uint8ClampedArray.BYTES_PER_ELEMENT === 1");
            assert.areEqual(2, Int16Array.BYTES_PER_ELEMENT, "Int16Array.BYTES_PER_ELEMENT === 2");
            assert.areEqual(2, Uint16Array.BYTES_PER_ELEMENT, "Uint16Array.BYTES_PER_ELEMENT === 2");
            assert.areEqual(4, Int32Array.BYTES_PER_ELEMENT, "Int32Array.BYTES_PER_ELEMENT === 4");
            assert.areEqual(4, Uint32Array.BYTES_PER_ELEMENT, "Uint32Array.BYTES_PER_ELEMENT === 4");
            assert.areEqual(4, Float32Array.BYTES_PER_ELEMENT, "Float32Array.BYTES_PER_ELEMENT === 4");
            assert.areEqual(8, Float64Array.BYTES_PER_ELEMENT, "Float64Array.BYTES_PER_ELEMENT === 8");

            verifyTypedArrayConstructorPropertyValue(Int8Array, "BYTES_PER_ELEMENT", "number", false);
            verifyTypedArrayConstructorPropertyValue(Uint8Array, "BYTES_PER_ELEMENT", "number", false);
            verifyTypedArrayConstructorPropertyValue(Uint8ClampedArray, "BYTES_PER_ELEMENT", "number", false);
            verifyTypedArrayConstructorPropertyValue(Int16Array, "BYTES_PER_ELEMENT", "number", false);
            verifyTypedArrayConstructorPropertyValue(Uint16Array, "BYTES_PER_ELEMENT", "number", false);
            verifyTypedArrayConstructorPropertyValue(Int32Array, "BYTES_PER_ELEMENT", "number", false);
            verifyTypedArrayConstructorPropertyValue(Uint32Array, "BYTES_PER_ELEMENT", "number", false);
            verifyTypedArrayConstructorPropertyValue(Float32Array, "BYTES_PER_ELEMENT", "number", false);
            verifyTypedArrayConstructorPropertyValue(Float64Array, "BYTES_PER_ELEMENT", "number", false);

            assert.areEqual(3, Int8Array.length, "Int8Array.length === 3");
            assert.areEqual(3, Uint8Array.length, "Uint8Array.length === 3");
            assert.areEqual(3, Uint8ClampedArray.length, "Uint8ClampedArray.length === 3");
            assert.areEqual(3, Int16Array.length, "Int16Array.length === 3");
            assert.areEqual(3, Uint16Array.length, "Uint16Array.length === 3");
            assert.areEqual(3, Int32Array.length, "Int32Array.length === 3");
            assert.areEqual(3, Uint32Array.length, "Uint32Array.length === 3");
            assert.areEqual(3, Float32Array.length, "Float32Array.length === 3");
            assert.areEqual(3, Float64Array.length, "Float64Array.length === 3");

            verifyTypedArrayConstructorPropertyValue(Int8Array, "length", "number",false);
            verifyTypedArrayConstructorPropertyValue(Uint8Array, "length", "number",false);
            verifyTypedArrayConstructorPropertyValue(Uint8ClampedArray, "length", "number", false);
            verifyTypedArrayConstructorPropertyValue(Int16Array, "length", "number", false);
            verifyTypedArrayConstructorPropertyValue(Uint16Array, "length", "number", false);
            verifyTypedArrayConstructorPropertyValue(Int32Array, "length", "number", false);
            verifyTypedArrayConstructorPropertyValue(Uint32Array, "length", "number", false);
            verifyTypedArrayConstructorPropertyValue(Float32Array, "length", "number", false);
            verifyTypedArrayConstructorPropertyValue(Float64Array, "length", "number", false);

            verifyTypedArrayConstructorPropertyValue(Int8Array, "prototype", "object", false);
            verifyTypedArrayConstructorPropertyValue(Uint8Array, "prototype", "object", false);
            verifyTypedArrayConstructorPropertyValue(Uint8ClampedArray, "prototype", "object", false);
            verifyTypedArrayConstructorPropertyValue(Int16Array, "prototype", "object", false);
            verifyTypedArrayConstructorPropertyValue(Uint16Array, "prototype", "object", false);
            verifyTypedArrayConstructorPropertyValue(Int32Array, "prototype", "object", false);
            verifyTypedArrayConstructorPropertyValue(Uint32Array, "prototype", "object", false);
            verifyTypedArrayConstructorPropertyValue(Float32Array, "prototype", "object", false);
            verifyTypedArrayConstructorPropertyValue(Float64Array, "prototype", "object", false);

            function verifyTypedArrayConstructorDoesNotHaveOwnProperty(name, displayName) {
                if(displayName === undefined)
                    displayName = name;

                assert.isFalse(Int8Array.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
                assert.isFalse(Uint8Array.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
                assert.isFalse(Uint8ClampedArray.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
                assert.isFalse(Int16Array.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
                assert.isFalse(Uint16Array.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
                assert.isFalse(Int32Array.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
                assert.isFalse(Uint32Array.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
                assert.isFalse(Float32Array.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
                assert.isFalse(Float64Array.hasOwnProperty(name), "TypedArray constructors don't have own property named '" + displayName + "'");
            }

            verifyTypedArrayConstructorDoesNotHaveOwnProperty('to');
            verifyTypedArrayConstructorDoesNotHaveOwnProperty('from');
            verifyTypedArrayConstructorDoesNotHaveOwnProperty(Symbol.create, "@@create");
        }
    },
    {
        name: "TypedArray prototypes have correct properties",
        body: function () {
            function verifyTypedArrayPrototypePropertyValue(constructor, name, type) {
                var descriptor = Object.getOwnPropertyDescriptor(constructor.prototype, name);
                var propName = constructor.name + ".prototype." + name;

                assert.isFalse(descriptor.writable, propName + ".writable === false");
                assert.isFalse(descriptor.enumerable, propName + ".enumerable === false");
                assert.isFalse(descriptor.configurable, propName + ".configurable === false");
                assert.areEqual(type, typeof descriptor.value, "typeof " + propName + " === '" + type + "'");
            }

            assert.areEqual(1, Int8Array.prototype.BYTES_PER_ELEMENT, "Int8Array.prototype.BYTES_PER_ELEMENT === 1");
            assert.areEqual(1, Uint8Array.prototype.BYTES_PER_ELEMENT, "Uint8Array.prototype.BYTES_PER_ELEMENT === 1");
            assert.areEqual(1, Uint8ClampedArray.prototype.BYTES_PER_ELEMENT, "Uint8ClampedArray.prototype.BYTES_PER_ELEMENT === 1");
            assert.areEqual(2, Int16Array.prototype.BYTES_PER_ELEMENT, "Int16Array.prototype.BYTES_PER_ELEMENT === 2");
            assert.areEqual(2, Uint16Array.prototype.BYTES_PER_ELEMENT, "Uint16Array.prototype.BYTES_PER_ELEMENT === 2");
            assert.areEqual(4, Int32Array.prototype.BYTES_PER_ELEMENT, "Int32Array.prototype.BYTES_PER_ELEMENT === 4");
            assert.areEqual(4, Uint32Array.prototype.BYTES_PER_ELEMENT, "Uint32Array.prototype.BYTES_PER_ELEMENT === 4");
            assert.areEqual(4, Float32Array.prototype.BYTES_PER_ELEMENT, "Float32Array.prototype.BYTES_PER_ELEMENT === 4");
            assert.areEqual(8, Float64Array.prototype.BYTES_PER_ELEMENT, "Float64Array.prototype.BYTES_PER_ELEMENT === 8");

            verifyTypedArrayPrototypePropertyValue(Int8Array, "BYTES_PER_ELEMENT", "number");
            verifyTypedArrayPrototypePropertyValue(Uint8Array, "BYTES_PER_ELEMENT", "number");
            verifyTypedArrayPrototypePropertyValue(Uint8ClampedArray, "BYTES_PER_ELEMENT", "number");
            verifyTypedArrayPrototypePropertyValue(Int16Array, "BYTES_PER_ELEMENT", "number");
            verifyTypedArrayPrototypePropertyValue(Uint16Array, "BYTES_PER_ELEMENT", "number");
            verifyTypedArrayPrototypePropertyValue(Int32Array, "BYTES_PER_ELEMENT", "number");
            verifyTypedArrayPrototypePropertyValue(Uint32Array, "BYTES_PER_ELEMENT", "number");
            verifyTypedArrayPrototypePropertyValue(Float32Array, "BYTES_PER_ELEMENT", "number");
            verifyTypedArrayPrototypePropertyValue(Float64Array, "BYTES_PER_ELEMENT", "number");

            assert.areEqual(Int8Array, Int8Array.prototype.constructor, "Int8Array.prototype.constructor === Int8Array");
            assert.areEqual(Uint8Array, Uint8Array.prototype.constructor, "Uint8Array.prototype.constructor === Uint8Array");
            assert.areEqual(Uint8ClampedArray, Uint8ClampedArray.prototype.constructor, "Uint8ClampedArray.prototype.constructor === Uint8ClampedArray");
            assert.areEqual(Int16Array, Int16Array.prototype.constructor, "Int16Array.prototype.constructor === Int16Array");
            assert.areEqual(Uint16Array, Uint16Array.prototype.constructor, "Uint16Array.prototype.constructor === Uint16Array");
            assert.areEqual(Int32Array, Int32Array.prototype.constructor, "Int32Array.prototype.constructor === Int32Array");
            assert.areEqual(Uint32Array, Uint32Array.prototype.constructor, "Uint32Array.prototype.constructor === Uint32Array");
            assert.areEqual(Float32Array, Float32Array.prototype.constructor, "Float32Array.prototype.constructor === Float32Array");
            assert.areEqual(Float64Array, Float64Array.prototype.constructor, "Float64Array.prototype.constructor === Float64Array");

            function verifyTypedArrayPrototypeDoesNotHaveOwnProperty(name, nameDesc) {
                nameDesc = nameDesc || name;
                assert.isFalse(Int8Array.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
                assert.isFalse(Uint8Array.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
                assert.isFalse(Uint8ClampedArray.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
                assert.isFalse(Int16Array.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
                assert.isFalse(Uint16Array.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
                assert.isFalse(Int32Array.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
                assert.isFalse(Uint32Array.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
                assert.isFalse(Float32Array.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
                assert.isFalse(Float64Array.prototype.hasOwnProperty(name), "TypedArray prototypes don't have own property named '" + nameDesc + "'");
            }

            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('set');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('subarray');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('copyWithin');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('fill');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('map');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('indexOf');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('includes');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('forEach');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('every');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('filter');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('find');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('findIndex');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('join');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('lastIndexOf');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('reduce');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('reduceRight');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('reverse');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('slice');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('some');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('sort');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('buffer');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('byteLength');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('byteOffset');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('length');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty(Symbol.toStringTag, '@@toStringTag');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('entries');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('keys');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('values');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty(Symbol.iterator, '@@iterator');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('toString');
            verifyTypedArrayPrototypeDoesNotHaveOwnProperty('toLocaleString');
        }
    },
    {
        name: "TypedArray instances have the correct properties",
        body: function() {
            var int8Array = new Int8Array(20);
            var uint8Array = new Uint8Array(20);
            var uint8ClampedArray = new Uint8ClampedArray(20);
            var uint16Array = new Uint16Array(20);
            var int16Array = new Int16Array(20);
            var int32Array = new Int32Array(20);
            var uint32Array = new Uint32Array(20);
            var float32Array = new Float32Array(20);
            var float64Array = new Float64Array(20);

            function verifyInstancesDoNotHaveOwnProperty(propertyName, propertyDesc) {
                propertyDesc = propertyDesc || propertyName;

                assert.isFalse(int8Array.hasOwnProperty(propertyName), "new Int8Array(20).hasOwnProperty('" + propertyDesc + "') === false");
                assert.isFalse(uint8Array.hasOwnProperty(propertyName), "new Uint8Array(20).hasOwnProperty('" + propertyDesc + "') === false");
                assert.isFalse(uint8ClampedArray.hasOwnProperty(propertyName), "new Uint8ClampedArray(20).hasOwnProperty('" + propertyDesc + "') === false");
                assert.isFalse(int16Array.hasOwnProperty(propertyName), "new Int16Array(20).hasOwnProperty('" + propertyDesc + "') === false");
                assert.isFalse(uint16Array.hasOwnProperty(propertyName), "new Uint16Array(20).hasOwnProperty('" + propertyDesc + "') === false");
                assert.isFalse(int32Array.hasOwnProperty(propertyName), "new Int32Array(20).hasOwnProperty('" + propertyDesc + "') === false");
                assert.isFalse(uint32Array.hasOwnProperty(propertyName), "new Uint32Array(20).hasOwnProperty('" + propertyDesc + "') === false");
                assert.isFalse(float32Array.hasOwnProperty(propertyName), "new Float32Array(20).hasOwnProperty('" + propertyDesc + "') === false");
                assert.isFalse(float64Array.hasOwnProperty(propertyName), "new Float64Array(20).hasOwnProperty('" + propertyDesc + "') === false");
            }

            // The accessors for buffer, byteLength, byteOffset, and length are inherited from prototype and are not instance properties
            verifyInstancesDoNotHaveOwnProperty('buffer');
            verifyInstancesDoNotHaveOwnProperty('byteLength');
            verifyInstancesDoNotHaveOwnProperty('byteOffset');
            verifyInstancesDoNotHaveOwnProperty('length');
            verifyInstancesDoNotHaveOwnProperty(Symbol.toStringTag, '@@toStringTag');
        }
    },
    {
        name: "Properties shared between Array.prototype and %TypedArray%.prototype",
        body: function() {
            var typedArrayPrototype = Int8Array.prototype.__proto__;

            assert.areEqual(Array.prototype.toString, typedArrayPrototype.toString, "Array.prototype.toString === %TypedArray%.prototype.toString");
            assert.areEqual(Array.prototype.toLocaleString, typedArrayPrototype.toLocaleString, "Array.prototype.toLocaleString === %TypedArray%.prototype.toLocaleString");
        }
    },
    {
        name: "%TypedArrayPrototype%.buffer behavior",
        body: function() {
            var typedArrayPrototype = Int8Array.prototype.__proto__;
            var descriptor = Object.getOwnPropertyDescriptor(typedArrayPrototype, 'buffer');

            assert.throws(function () { typedArrayPrototype.buffer; }, TypeError, "%TypedArrayPrototype%.buffer throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get(); }, TypeError, "%TypedArrayPrototype%.buffer throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(); }, TypeError, "%TypedArrayPrototype%.buffer throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(undefined); }, TypeError, "%TypedArrayPrototype%.buffer throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call([1,2,3]); }, TypeError, "%TypedArrayPrototype%.buffer throws TypeError if called with a non-TypedArray parameter", "'this' is not a typed array object");

            var buf = new ArrayBuffer(100);
            assert.areEqual(buf, new Uint8Array(buf).buffer, "new Uint8Array(buf).buffer === buf");
            assert.areEqual(20, new Uint8Array(20).buffer.byteLength, "new Uint8Array(20).buffer.byteLength === 20");
            assert.areEqual(buf, descriptor.get.call(new Float32Array(buf)), "TypedArray.prototype.buffer can be used to retrieve the ArrayBuffer of a different TypedArray type");
        }
    },
    {
        name: "%TypedArrayPrototype%.byteLength behavior",
        body: function() {
            var typedArrayPrototype = Int8Array.prototype.__proto__;
            var descriptor = Object.getOwnPropertyDescriptor(typedArrayPrototype, 'byteLength');

            assert.throws(function () { typedArrayPrototype.byteLength; }, TypeError, "%TypedArrayPrototype%.byteLength throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get(); }, TypeError, "%TypedArrayPrototype%.byteLength throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(); }, TypeError, "%TypedArrayPrototype%.byteLength throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(undefined); }, TypeError, "%TypedArrayPrototype%.byteLength throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call([1,2,3]); }, TypeError, "%TypedArrayPrototype%.byteLength throws TypeError if called with a non-TypedArray parameter", "'this' is not a typed array object");

            var buf = new ArrayBuffer(100);
            assert.areEqual(100, new Uint8Array(buf).byteLength, "new Uint8Array(buf).byteLength === 100");
            assert.areEqual(80, new Uint32Array(20).byteLength, "new Uint8Array(20).byteLength === 80");
            assert.areEqual(100, descriptor.get.call(new Float32Array(buf)), "TypedArray.prototype.byteLength can be used to retrieve the byte length of a different TypedArray type");
        }
    },
    {
        name: "%TypedArrayPrototype%.byteOffset behavior",
        body: function() {
            var typedArrayPrototype = Int8Array.prototype.__proto__;
            var descriptor = Object.getOwnPropertyDescriptor(typedArrayPrototype, 'byteOffset');

            assert.throws(function () { typedArrayPrototype.byteOffset; }, TypeError, "%TypedArrayPrototype%.byteOffset throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get(); }, TypeError, "%TypedArrayPrototype%.byteOffset throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(); }, TypeError, "%TypedArrayPrototype%.byteOffset throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(undefined); }, TypeError, "%TypedArrayPrototype%.byteOffset throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call([1,2,3]); }, TypeError, "%TypedArrayPrototype%.byteOffset throws TypeError if called with a non-TypedArray parameter", "'this' is not a typed array object");

            var buf = new ArrayBuffer(100);
            assert.areEqual(0, new Uint8Array(buf).byteOffset, "new Uint8Array(buf).byteOffset === 0");
            assert.areEqual(0, new Uint16Array(20).byteOffset, "new Uint16Array(20).byteOffset === 0");
            assert.areEqual(8, new Uint32Array(buf, 8).byteOffset, "new Uint8Array(buf).byteOffset === 8");
            assert.areEqual(0, descriptor.get.call(new Float32Array(buf)), "TypedArray.prototype.byteOffset can be used to retrieve the byte offset of a different TypedArray type");
        }
    },
    {
        name: "%TypedArrayPrototype%.length behavior",
        body: function() {
            var typedArrayPrototype = Int8Array.prototype.__proto__;
            var descriptor = Object.getOwnPropertyDescriptor(typedArrayPrototype, 'length');

            assert.throws(function () { typedArrayPrototype.length; }, TypeError, "%TypedArrayPrototype%.length throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get(); }, TypeError, "%TypedArrayPrototype%.length throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(); }, TypeError, "%TypedArrayPrototype%.length throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(undefined); }, TypeError, "%TypedArrayPrototype%.length throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call([1,2,3]); }, TypeError, "%TypedArrayPrototype%.length throws TypeError if called with a non-TypedArray parameter", "'this' is not a typed array object");

            var buf = new ArrayBuffer(100);
            assert.areEqual(100, new Uint8Array(buf).length, "new Uint8Array(buf).length === 100");
            assert.areEqual(20, new Uint32Array(20).length, "new Uint32Array(20).length === 20");
            assert.areEqual(50, descriptor.get.call(new Uint16Array(buf)), "TypedArray.prototype.length can be used to retrieve the length of a different TypedArray type");
        }
    },
    {
        name: "%TypedArrayPrototype%[@@toStringTag] behavior",
        body: function() {
            var typedArrayPrototype = Int8Array.prototype.__proto__;
            var descriptor = Object.getOwnPropertyDescriptor(typedArrayPrototype, Symbol.toStringTag);

            assert.throws(function () { typedArrayPrototype.length; }, TypeError, "%TypedArrayPrototype%[@@toStringTag] throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get(); }, TypeError, "%TypedArrayPrototype%[@@toStringTag] throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(); }, TypeError, "%TypedArrayPrototype%[@@toStringTag] throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call(undefined); }, TypeError, "%TypedArrayPrototype%[@@toStringTag] throws TypeError if called with no parameter", "'this' is not a typed array object");
            assert.throws(function () { descriptor.get.call([1,2,3]); }, TypeError, "%TypedArrayPrototype%[@@toStringTag] throws TypeError if called with a non-TypedArray parameter", "'this' is not a typed array object");

            assert.areEqual('Int8Array', new Int8Array(10)[Symbol.toStringTag], "new Int8Array()[@@toStringTag] === 'Int8Array'");
            assert.areEqual('Uint8Array', new Uint8Array(10)[Symbol.toStringTag], "new Uint8Array()[@@toStringTag] === 'Uint8Array'");
            assert.areEqual('Uint8ClampedArray', new Uint8ClampedArray(10)[Symbol.toStringTag], "new Uint8ClampedArray()[@@toStringTag] === 'Uint8ClampedArray'");
            assert.areEqual('Int16Array', new Int16Array(10)[Symbol.toStringTag], "new Int16Array()[@@toStringTag] === 'Int16Array'");
            assert.areEqual('Uint16Array', new Uint16Array(10)[Symbol.toStringTag], "new Uint16Array()[@@toStringTag] === 'Uint16Array'");
            assert.areEqual('Int32Array', new Int32Array(10)[Symbol.toStringTag], "new Int32Array()[@@toStringTag] === 'Int32Array'");
            assert.areEqual('Uint32Array', new Uint32Array(10)[Symbol.toStringTag], "new Uint32Array()[@@toStringTag] === 'Uint32Array'");
            assert.areEqual('Float32Array', new Float32Array(10)[Symbol.toStringTag], "new Float32Array()[@@toStringTag] === 'Float32Array'");
            assert.areEqual('Float64Array', new Float64Array(10)[Symbol.toStringTag], "new Float64Array()[@@toStringTag] === 'Float64Array'");
        }
    },
    {
        name: "%TypedArray%.from basic behavior",
        body: function() {
            assert.areEqual([0,1,2,3], Uint8Array.from([0,1,2,3]), "%TypedArray%.from basic behavior with an iterable object");
            assert.areEqual([0,1,2,3], Uint8Array.from({ 0: 0, 1: 1, 2: 2, 3: 3, length: 4 }), "%TypedArray%.from basic behavior with an object with length but no iterator");
        }
    },
    {
        name: "%TypedArray%.from extended behavior",
        body: function() {
            var fromFnc = Uint8Array.__proto__.from;

            var b = fromFnc.call(Array, "string");
            assert.areEqual('object', typeof b, "%TypedArray%.from.call(Array, 'string') returns an array");
            assert.areEqual(['s','t','r','i','n','g'], b, "%TypedArray%.from.call(Array, 'string') == ['s','t','r','i','n','g']");
            assert.areEqual(6, b.length, "%TypedArray%.from.call(Array, 'string').length === 6");
            assert.isFalse(ArrayBuffer.isView(b), "%TypedArray%.from.call(Array, 'string') is not a TypedArray");

            var a = { 0: 0, 1: 1, 2: 2, length: 5 };
            var b = fromFnc.call(Uint8Array, a);
            assert.areEqual(5, b.length, "Uint8Array.from(objectWithLengthProperty) returns a new Uint8Array with length = a.length");
            assert.isTrue(ArrayBuffer.isView(b), "Uint8Array.from(objectWithLengthProperty) returns a TypedArray (ArrayBuffer.isView)");
            assert.areEqual(5, b.byteLength, "Uint8Array.from(objectWithLengthProperty) returns a TypedArray (with correct byteLength)");
            assert.areEqual([0,1,2,0,0], b, "Uint8Array.from(objectWithLengthProperty) has missing values set to 0");

            var a = { 0: 0, 1: 1, 2: 2, length: 5 };
            var b = fromFnc.call(Array, a);
            assert.areEqual('object', typeof b, "%TypedArray%.from.call(Array, objectWithLengthProperty) returns an object");
            assert.areEqual(5, b.length, "%TypedArray%.from.call(Array, objectWithLengthProperty) returns a new array with length = a.length");
            assert.isFalse(ArrayBuffer.isView(b), "%TypedArray%.from.call(Array, objectWithLengthProperty) is not a TypedArray (ArrayBuffer.isView)");
            assert.areEqual([0,1,2,undefined,undefined], b, "%TypedArray%.from.call(Array, objectWithLengthProperty) has missing values set to undefined");

            var a = { 0: 0, 1: 1 };
            var b = fromFnc.call(Array, a);
            assert.areEqual('object', typeof b, "%TypedArray%.from.call(Array, objectWithLengthNoProperty) returns an object");
            assert.areEqual(0, b.length, "%TypedArray%.from.call(Array, objectWithLengthNoProperty) returns a new array with length = 0");
            assert.isFalse(ArrayBuffer.isView(b), "%TypedArray%.from.call(Array, objectWithLengthNoProperty) is not a TypedArray (ArrayBuffer.isView)");
            assert.areEqual([], b, "%TypedArray%.from.call(Array, objectWithLengthNoProperty) is an empty array");

            assert.throws(function () { fromFnc.call(); }, TypeError, "Calling %TypedArray%.from with no this throws TypeError", "[TypedArray].from: 'this' is not a Function object");
            assert.throws(function () { fromFnc.call(undefined); }, TypeError, "Calling %TypedArray%.from with undefined this throws TypeError", "[TypedArray].from: 'this' is not a Function object");
            assert.throws(function () { fromFnc.call('string'); }, TypeError, "Calling %TypedArray%.from with non-function this throws TypeError", "[TypedArray].from: 'this' is not a Function object");
            assert.throws(function () { fromFnc.call(Math.sin, []); }, TypeError, "Calling %TypedArray%.from with non-constructor this function throws TypeError", "[TypedArray].from: 'this' is not a Function object");
            assert.throws(function () { Uint8Array.from(); }, TypeError, "Calling %TypedArray%.from with non-object items argument throws TypeError", "[TypedArray].from: argument is not an Object");
            assert.throws(function () { Uint8Array.from(undefined); }, TypeError, "Calling %TypedArray%.from with non-object items argument throws TypeError", "[TypedArray].from: argument is not an Object");
            assert.throws(function () { Uint8Array.from(null); }, TypeError, "Calling %TypedArray%.from with non-object items argument throws TypeError", "[TypedArray].from: argument is not an Object");
            assert.throws(function () { Uint8Array.from({}, undefined); }, TypeError, "Calling %TypedArray%.from with non-object mapFn argument throws TypeError", "[TypedArray].from: argument is not a Function object");
            assert.throws(function () { Uint8Array.from({}, null); }, TypeError, "Calling %TypedArray%.from with non-object mapFn argument throws TypeError", "[TypedArray].from: argument is not a Function object");
            assert.throws(function () { Uint8Array.from({}, 'string'); }, TypeError, "Calling %TypedArray%.from with non-object mapFn argument throws TypeError", "[TypedArray].from: argument is not a Function object");
            assert.throws(function () { Uint8Array.from({}, {}); }, TypeError, "Calling %TypedArray%.from with non-function mapFn argument throws TypeError", "[TypedArray].from: argument is not a Function object");
            assert.throws(function () { fromFnc.call(String, [0,1,2,3]); }, TypeError, "Calling %TypedArray%.from with no this throws TypeError", "Cannot create property for a non-extensible object");
        }
    },
    {
        name: "%TypedArray%.from calls down into correct TypedArray SetItem API",
        body: function() {
            var u = Uint8ClampedArray.from([0,-1,2,300]);
            assert.areEqual(4, u.length, "Uint8ClampedArray.from([0,-1,2,300]) returns a new Uint8ClampedArray with length = 4");
            assert.isTrue(ArrayBuffer.isView(u), "Uint8ClampedArray.from([0,-1,2,300]) returns a TypedArray (ArrayBuffer.isView)");
            assert.areEqual(4, u.byteLength, "Uint8ClampedArray.from([0,-1,2,300]) returns a TypedArray (with correct byteLength)");
            assert.areEqual([0,0,2,255], u, "Uint8ClampedArray.from([0,-1,2,300]) has the correct values");
        }
    },
    {
        name: "%TypedArray%.from behavior with iterable source item",
        body: function() {
            var fromFnc = Uint8Array.__proto__.from;
            var objectWithIterator = {
                [Symbol.iterator]: function() {
                    return {
                        i: 0,
                        next: function () {
                            return {
                                done: this.i == 5,
                                value: this.i++
                            };
                        }
                    };
                }
            };

            var b = fromFnc.call(Uint8Array, objectWithIterator);
            assert.areEqual(5, b.length, "%TypedArray%.from.call(Uint8Array, objectWithIterator) returns a new Uint8Array with length = objectWithIterator.length");
            assert.isTrue(ArrayBuffer.isView(b), "%TypedArray%.from(objectWithIterator) returns a TypedArray (ArrayBuffer.isView)");
            assert.areEqual(5, b.byteLength, "%TypedArray%.from(objectWithIterator) returns a TypedArray (with correct byteLength)");
            assert.areEqual([0,1,2,3,4], b, "%TypedArray%.from(objectWithIterator) has correct values");
        }
    },
    {
        name: "%TypedArray%.from behavior with iterable source item which doesn't hold references to objects it returns",
        body: function() {
            var fromFnc = Uint8Array.__proto__.from;
            var objectWithIterator = {
                [Symbol.iterator]: function() {
                    return {
                        i: 0,
                        next: function () {
                            CollectGarbage();
                            return {
                                done: this.i == 10,
                                value: {
                                    myi: this.i++
                                }
                            };
                        }
                    };
                }
            };

            var b = fromFnc.call(Array, objectWithIterator);
            assert.areEqual(10, b.length, "%TypedArray%.from.call(Array, objectWithIterator) returns a new Array with length = objectWithIterator.length");
            for (var i = 0; i < b.length; i++) {
                assert.isTrue(b[i] !== undefined, "Object at index " + i + " is not undefined");
                assert.areEqual('object', typeof b[i], "Object at index " + i + " is an object");
                assert.areEqual(i, b[i].myi, "Object at index " + i + " has correct value");
            }
        }
    },
    // {
        // name: "%TypedArray%.from order of operations test via proxy traps",
        // body: function() {
            // var fromFnc = Uint8Array.__proto__.from;
            // var obj = [0, 1, 2, 3, 4];
            // var currentIndex = 0;
            // var proxyHandler = {
                // get: function(target, name, receiver) {
                    // assert.isTrue(currentIndex < name, "We should be accessing the elements of obj in order.");
                    // return receiver[name];
                // }
            // };
            // var proxyObj = Proxy(obj, proxyHandler);

            // var b = fromFnc.call(Array, proxyObj);
            // assert.areEqual(10, b.length, "%TypedArray%.from(proxyObj) returns a new Array with length = obj.length");
            // for (var i = 0; i < b.length; i++) {
                // assert.isTrue(b[i] !== undefined, "Object at index " + i + " is not undefined");
                // assert.areEqual('object', typeof b[i], "Object at index " + i + " is an object");
                // assert.areEqual(i, b[i].myi, "Object at index " + i + " has correct value");
            // }
        // }
    // },
    {
        name: "%TypedArray%.from behavior with a map function",
        body: function() {
            var i = 0;

            function mapFunction(val, k) {
                assert.areEqual(i, k, "%TypedArray%.from called with a mapping function, we should get the elements in order. Setting item[" + k + "] = " + val);
                assert.areEqual(val, k, "Value and index should be same for this test");
                assert.areEqual(2, arguments.length, "%TypedArray%.from called with a mapping function, only 2 arguments should be passed to the map function");

                // increment expected index
                i++;
            }

            var objectWithoutIterator = {
                0: 0,
                1: 1,
                2: 2,
                3: 3,
                length: 4
            };

            // Verify mapFunction gets called with correct arguments
            Int8Array.from(objectWithoutIterator, mapFunction);
        }
    },
    {
        name: "%TypedArray%.from behavior with a map function passing this argument",
        body: function() {
            var thisArg = 'thisArg';

            function mapFunction(val, k) {
                // this will be wrapped as an Object
                assert.areEqual(Object(thisArg), this, "thisArg passed into %TypedArray%.from should flow into mapFunction");
            }

            var objectWithoutIterator = {
                0: 0,
                1: 1,
                2: 2,
                3: 3,
                length: 4
            };

            // Verify mapFunction gets called with thisArg passed as this
            Int8Array.from(objectWithoutIterator, mapFunction, thisArg);
        }
    },
    {
        name: "%TypedArray%.from behavior with a map function that mutates source object",
        body: function() {
            var objectWithoutIterator = {
                0: 0,
                1: 1,
                2: 2,
                3: 3,
                4: 4,
                length: 5
            };

            function mapFunction(val, k) {
                switch (k) {
                case 0:
                    // change the objectWithoutIterator length value - we should still fetch the rest of the indexed-elements anyway
                    objectWithoutIterator.length = 0;
                    return val;
                case 1:
                    // change the value of the next indexed value - the new value should end up in the return object
                    objectWithoutIterator[2] = 200;
                    return val;
                case 2:
                    // change the value of a previous indexed value - the old value should end up in the return object
                    objectWithoutIterator[0] = -100;
                    return val;
                case 3:
                    // delete the next indexed value - return object should have undefined there
                    delete objectWithoutIterator[4];
                    return val;
                }

                // otherwise
                return val;
            }

            assert.areEqual([0,1,200,3,0], Int32Array.from(objectWithoutIterator, mapFunction), "Int32Array.from called with a map function that mutates the source object");
        }
    },
    {
        name: "%TypedArray%.from behavior with iterator and a map function",
        body: function() {
            var j = 0;
            var checkThisArg = false;
            var thisArg = 'string';

            var objectWithIterator = {
                [Symbol.iterator]: function() {
                    return {
                        i: 0,
                        next: function () {
                            return {
                                done: this.i == 5,
                                value: this.i++
                            };
                        }
                    };
                }
            };

            function mapFunction(val, k) {
                assert.areEqual(j, val, "%TypedArray%.from called with a mapping function, we should get the elements in order. Setting item[" + j + "] = " + val);
                assert.areEqual(val, val, "%TypedArray%.from called with a mapping function, index should be equal to the value passed in.");
                assert.areEqual(2, arguments.length, "%TypedArray%.from called with a mapping function, only 2 arguments should be passed to the map function.");

                // increment expected value
                j++;

                if (checkThisArg) {
                    // this will be wrapped as an Object
                    assert.areEqual(Object(thisArg), this, "thisArg passed into %TypedArray%.from should flow into mapFunction");
                }
            }

            // Verify mapFunction gets called with correct arguments
            Int8Array.from(objectWithIterator, mapFunction);

            j = 0;
            checkThisArg = true;

            // Verify mapFunction gets called with thisArg passed as this
            Int8Array.from(objectWithIterator, mapFunction, thisArg);
        }
    },
    {
        name: "%TypedArray%.from behavior with iterator and a map function which mutates the iterator state",
        body: function() {
            var iterator_val = 0;

            var objectWithIterator = {
                [Symbol.iterator]: function() {
                    return {
                        next: function () {
                            return {
                                done: iterator_val == 5,
                                value: iterator_val++
                            };
                        }
                    };
                }
            };

            var reset = false;
            function mapFunction(val, k) {
                if (val == 2 && !reset)
                {
                    reset = true;
                    iterator_val = 0;
                }

                return val;
            }

            assert.areEqual([0,1,2,3,4], Int8Array.from(objectWithIterator, mapFunction), "Int8Array.from called with map function which alters iterator state");
        }
    },
    {
        name: "%TypedArray%.from behavior with badly formed iterator objects",
        body: function() {
            // Test GetIterator where obj[@@iterator] is not a function
            var objectWithIteratorThatIsNotAFunction = {
                [Symbol.iterator]: 'a string'
            };
            assert.throws(function() { Int8Array.from(objectWithIteratorThatIsNotAFunction); }, TypeError, "obj[@@iterator] must be a function", "Function expected");

            // Test GetIterator where obj[@@iterator] is a function which doesn't return an object
            var objectWithIteratorWhichDoesNotReturnObjects = {
                [Symbol.iterator]: function() {
                    return undefined;
                }
            };
            assert.throws(function() { Int8Array.from(objectWithIteratorWhichDoesNotReturnObjects); }, TypeError, "obj[@@iterator] must return an object", "Object expected");

            // Test IteratorNext where obj[@@iterator].next is not a function
            var objectWithIteratorNextIsNotAFunction = {
                [Symbol.iterator]: function() {
                    return {
                        next: undefined
                    };
                }
            };
            assert.throws(function() { Int8Array.from(objectWithIteratorNextIsNotAFunction); }, TypeError, "obj[@@iterator].next must be a function", "Function expected");

            // Test IteratorNext where obj[@@iterator].next doesn't return an object
            var objectWithIteratorNextDoesNotReturnObjects = {
                [Symbol.iterator]: function() {
                    return {
                        next: function() {
                            return undefined;
                        }
                    };
                }
            };
            assert.throws(function() { Int8Array.from(objectWithIteratorNextDoesNotReturnObjects); }, TypeError, "obj[@@iterator].next must return an object", "Object expected");
        }
    },
    {
        name: "%TypedArray%.of basic behavior",
        body: function() {
            assert.areEqual([], Uint8Array.of(), "%TypedArray%.of basic behavior with no arguments");
            assert.areEqual([3], Uint8Array.of(3), "%TypedArray%.of basic behavior with a single argument");
            assert.areEqual([0,1,2,3], Uint8Array.of(0, 1, 2, 3), "%TypedArray%.of basic behavior with a list of arguments");
        }
    },
    {
        name: "%TypedArray%.of extended behavior",
        body: function() {
            var ofFnc = Uint8Array.__proto__.of;

            var u = ofFnc.call(Uint8ClampedArray, 0, -1, 2, 300, 4);
            assert.areEqual(5, u.length, "Uint8ClampedArray.of(0, -1, 2, 300, 4) returns a new Uint8ClampedArray with length = 5");
            assert.isTrue(ArrayBuffer.isView(u), "Uint8ClampedArray.of(0, -1, 2, 300, 4) returns a TypedArray (ArrayBuffer.isView)");
            assert.areEqual(5, u.byteLength, "Uint8ClampedArray.of(0, -1, 2, 300, 4) returns a TypedArray (with correct byteLength)");
            assert.areEqual([0,0,2,255,4], u, "Uint8ClampedArray.of(0, -1, 2, 300, 4) has the correct values");

            var b = ofFnc.call(Array, 'string', 'other string', 5, { 0: 'string val',length:10 });
            assert.areEqual('object', typeof b, "%TypedArray%.of.call(Array, ...someStringsAndObjects) returns an array");
            assert.areEqual(['string','other string',5,{ 0: 'string val',length:10 }], b, "%TypedArray%.of.call(Array, ...someStringsAndObjects) == ['string','other string',5,{ 0: 'string val',length:10 }]");
            assert.areEqual(4, b.length, "%TypedArray%.of.call(Array, ...someStringsAndObjects).length === 4");
            assert.isFalse(ArrayBuffer.isView(b), "%TypedArray%.of.call(Array, ...someStringsAndObjects) is not a TypedArray");

            var b = ofFnc.call(String, 0, 1, 2, 3);
            assert.areEqual('object', typeof b, "%TypedArray%.of.call(String, 0, 1, 2, 3) returns a string object");
            assert.areEqual(1, b.length, "%TypedArray%.of.call(String, 0, 1, 2, 3) returns a string object with length 1");
            assert.areEqual('4', b.toString(), "%TypedArray%.of.call(String, 0, 1, 2, 3) returns a string object with value == '4'");
            assert.areEqual('4', b[0], "%TypedArray%.of.call(String, 0, 1, 2, 3) returns a string object s. s[0] == '4'");
            assert.areEqual(1, b[1], "%TypedArray%.of.call(String, 0, 1, 2, 3) returns a string object s. s[1] == 1");
            assert.areEqual(2, b[2], "%TypedArray%.of.call(String, 0, 1, 2, 3) returns a string object s. s[2] == 2");
            assert.areEqual(3, b[3], "%TypedArray%.of.call(String, 0, 1, 2, 3) returns a string object s. s[3] == 3");

            assert.areEqual([], ofFnc.call(Array), "%TypedArray%.of.call(Array) returns empty array");
            assert.areEqual(new String(0), ofFnc.call(String), "%TypedArray%.of.call(String) returns empty string");
            assert.areEqual("0", ofFnc.call(String).toString(), "%TypedArray%.of.call(String) returns empty string");

            var b = Uint8Array.of();
            assert.areEqual(0, b.length, "Uint8Array.of() returns empty Uint8Array");
            assert.areEqual(0, b.byteLength, "Uint8Array.of() returns empty Uint8Array");
            assert.isTrue(ArrayBuffer.isView(b), "Uint8Array.of() returns actual TypedArray");
            assert.areEqual(Uint8Array, b.constructor, "Uint8Array.of() returns correct TypedArray type");

            assert.throws(function () { ofFnc.call(); }, TypeError, "Calling %TypedArray%.of with no this throws TypeError", "[TypedArray].of: 'this' is not a Function object");
            assert.throws(function () { ofFnc.call(undefined); }, TypeError, "Calling %TypedArray%.of with undefined this throws TypeError", "[TypedArray].of: 'this' is not a Function object");
            assert.throws(function () { ofFnc.call('string'); }, TypeError, "Calling %TypedArray%.of with non-object this throws TypeError", "[TypedArray].of: 'this' is not a Function object");
        }
    },
    {
        name: "%TypedArray%.prototype.copyWithin behavior",
        body: function() {
            function getTypedArray() {
                var t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i+1;
                }

                return t;
            }

            function getRegularArray() {
                return [1,2,3,4,5,6,7,8,9,10];
            }

            function getObjectArray() {
                return {
                    0:1, 1:2, 2:3, 3:4, 4:5, 5:6, 6:7, 7:8, 8:9, 9:10, length: 10
                };
            }

            function testMethod(copyWithinFn, getDataFn) {
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn()), "%TypedArrayPrototype%.copyWithin copying the entire array to the first index returns the same array");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0), "%TypedArrayPrototype%.copyWithin copying the entire array to the first index returns the same array");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0, 0), "%TypedArrayPrototype%.copyWithin copying the entire array to the first index returns the same array");
                assert.areEqual([6,7,8,9,10,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0, 5), "%TypedArrayPrototype%.copyWithin copying the entire array beginning at index 5 to the first index");
                assert.areEqual([6,7,8,9,10,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0, 5, 10), "%TypedArrayPrototype%.copyWithin copying the entire array beginning at index 5 to the first index");
                assert.areEqual([6,7,8,9,10,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0, 5, 11), "%TypedArrayPrototype%.copyWithin copying the entire array beginning at index 5 to the first index");
                assert.areEqual([6,2,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), -50, 5, 6), "%TypedArrayPrototype%.copyWithin copying a single element beginning at index 5 to the first index");
                assert.areEqual([6,7,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), -10, 5, 7), "%TypedArrayPrototype%.copyWithin copying two elements beginning at index 5 to the first index");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0, 5, 3), "%TypedArrayPrototype%.copyWithin copying elements where end > start causes no changes");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0, 5, 5), "%TypedArrayPrototype%.copyWithin copying elements where end = start causes no changes");
                assert.areEqual([6,7,8,9,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0, 5, 9), "%TypedArrayPrototype%.copyWithin copying elements where end = arr.length-1");
                assert.areEqual([6,7,8,9,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), 0, 5, -1), "%TypedArrayPrototype%.copyWithin copying elements where end -1 is the same as end === arr.length-1");
                assert.areEqual([1,2,3,4,5,6,7,8,9,6], copyWithinFn.call(getDataFn(), -1, 5), "%TypedArrayPrototype%.copyWithin copying from the middle of the array to the last index");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), -5, 5, -1), "%TypedArrayPrototype%.copyWithin copying the middle of the array to the last index - 1");
                assert.areEqual([1,6,7,8,9,6,7,8,9,10], copyWithinFn.call(getDataFn(), 1, -5, -1), "%TypedArrayPrototype%.copyWithin copying the middle of the array to the last index - 1");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), 1, 0, -10), "%TypedArrayPrototype%.copyWithin copying anything where end is 0 or less causes us to do nothing");
                assert.areEqual([1,1,2,4,5,6,7,8,9,10], copyWithinFn.call(getDataFn(), 1, -100, 2), "%TypedArrayPrototype%.copyWithin copying two elements starting at 0 into start position 1");
                assert.areEqual([2,3,4,5,6,7,8,9,10,10], copyWithinFn.call(getDataFn(), 0, -9), "%TypedArrayPrototype%.copyWithin copying all but the last elements starting at 1 into start position 0");
            }

            testMethod(Uint8Array.prototype.__proto__.copyWithin, getTypedArray);
            testMethod(Array.prototype.copyWithin, getRegularArray);
            testMethod(Array.prototype.copyWithin, getTypedArray);

            var copyWithinFn = Uint8Array.prototype.__proto__.copyWithin;

            assert.isTrue(ArrayBuffer.isView(copyWithinFn.call(getTypedArray(), 0, 0)), "%TypedArrayPrototype%.copyWithin returns a TypedArray");
            assert.areEqual("Int8Array", copyWithinFn.call(getTypedArray(), 0, 0)[Symbol.toStringTag], "%TypedArrayPrototype%.copyWithin returns the right TypedArray type");

            assert.isTrue(ArrayBuffer.isView(Array.prototype.copyWithin.call(getTypedArray(), 0, 0)), "Array.prototype.copyWithin returns a TypedArray when a TypedArray is passed in");
            assert.isFalse(ArrayBuffer.isView(Array.prototype.copyWithin.call(getRegularArray(), 0, 0)), "Array.prototype.copyWithin returns a normal array when a TypedArray is not passed in");
            assert.isFalse(ArrayBuffer.isView(Array.prototype.copyWithin.call(getObjectArray(), 0, 0)), "Array.prototype.copyWithin returns a normal array when a TypedArray is not passed in");

            assert.throws(function() { copyWithinFn.call(); }, TypeError, "Calling %TypedArrayPrototype%.copyWithin with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { copyWithinFn.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.copyWithin with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { copyWithinFn.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.copyWithin with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { copyWithinFn.call(getRegularArray()); }, TypeError, "Calling %TypedArrayPrototype%.copyWithin with non-TypedArray object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { copyWithinFn.call(getObjectArray()); }, TypeError, "Calling %TypedArrayPrototype%.copyWithin with non-TypedArray object this throws TypeError", "'this' is not a typed array object");
        }
    },
    {
        name: "%TypedArray%.prototype.fill behavior",
        body: function() {
            var fill = Uint8Array.prototype.__proto__.fill;

            function getTypedArray() {
                var t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i+1;
                }

                return t;
            }

            function testMethod(getDataFn) {
                assert.areEqual([0,0,0,0,0,0,0,0,0,0], fill.call(getDataFn(), 0), "%TypedArrayPrototype%.fill filling the entire array with 0");
                assert.areEqual([0,0,0,0,0,0,0,0,0,0], fill.call(getDataFn(), 0, undefined), "%TypedArrayPrototype%.fill should use length as the end value when undefined is specified");
                assert.areEqual([0,0,0,0,0,0,0,0,0,0], fill.call(getDataFn(), 0, 0), "%TypedArrayPrototype%.fill filling the entire array with 0");
                assert.areEqual([0,0,0,0,0,0,0,0,0,0], fill.call(getDataFn(), 0, 0, 100), "%TypedArrayPrototype%.fill filling the entire array with 0");
                assert.areEqual([0,0,0,0,0,0,0,0,0,0], fill.call(getDataFn(), 0, -50), "%TypedArrayPrototype%.fill filling the entire array with 0");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], fill.call(getDataFn(), 0, 100), "%TypedArrayPrototype%.fill filling no elements because start > length");
                assert.areEqual([1,2,3,4,5,0,0,0,0,0], fill.call(getDataFn(), 0, 5), "%TypedArrayPrototype%.fill filling all elements after index 5");
                assert.areEqual([0,0,0,0,0,6,7,8,9,10], fill.call(getDataFn(), 0, 0, 5), "%TypedArrayPrototype%.fill filling first 5 elements");
                assert.areEqual([1,2,3,0,5,6,7,8,9,10], fill.call(getDataFn(), 0, 3, 4), "%TypedArrayPrototype%.fill filling one element at index 3");
                assert.areEqual([1,2,3,4,5,6,7,0,0,10], fill.call(getDataFn(), 0, -3, -1), "%TypedArrayPrototype%.fill filling two elements near the end");
                assert.areEqual([1,2,3,4,5,6,7,8,9,0], fill.call(getDataFn(), 0, -1), "%TypedArrayPrototype%.fill filling the last element");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], fill.call(getDataFn(), 0, 5, 4), "%TypedArrayPrototype%.fill filling the no elements because start > end");
                assert.areEqual([1,2,3,4,5,6,7,8,9,10], fill.call(getDataFn(), 0, 4, 4), "%TypedArrayPrototype%.fill filling the no elements because start == end");
            }

            testMethod(getTypedArray);

            assert.throws(function() { fill.call(); }, TypeError, "Calling %TypedArrayPrototype%.fill with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { fill.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.fill with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { fill.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.fill with non-object this throws TypeError", "'this' is not a typed array object");
        }
    },
    {
        name: "%TypedArray%.prototype.map behavior",
        body: function() {
            var mapFn = Int8Array.prototype.__proto__.map;
            var counter = 0;
            var t;
            var thisArg = 'a string';

            function getTypedArray() {
                // Reset the counter var here
                counter = 0;
                // Save the latest array in t for use by the mapping function
                t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function mappingFn(elem, index, arr) {
                assert.areEqual(index, counter, "We call the mapping function on elements in the correct order");
                assert.areEqual(elem, index, "We pass the correct values to the mapping function");
                assert.isTrue(arr === t, "Array passed to the mapping function matches array map is called on");
                assert.areEqual(Object(thisArg), this, "This value passed to map is correct");

                // Increment counter var for the next index
                counter++;

                return -1 * elem;
            }

            function testMethod(testFn, getDataFn, isReturnTypedArray) {
                assert.areEqual([0,-1,-2,-3,-4,-5,-6,-7,-8,-9], testFn.call(getDataFn(), mappingFn, thisArg), "%TypedArrayPrototype%.map basic functionality of the mapping function");

                if (isReturnTypedArray) {
                    assert.isTrue(ArrayBuffer.isView(testFn.call(getDataFn(), mappingFn, thisArg)), "%TypedArrayPrototype%.map returns a TypedArray");
                    assert.areEqual("Int8Array", testFn.call(getDataFn(), mappingFn, thisArg)[Symbol.toStringTag], "%TypedArrayPrototype%.map returns the right TypedArray type");
                } else {
                    assert.isFalse(ArrayBuffer.isView(testFn.call(getDataFn(), mappingFn, thisArg)), "%TypedArrayPrototype%.map returns a normal array");
                }
            }

            testMethod(mapFn, getTypedArray, true);
            testMethod(Array.prototype.map, getTypedArray, false);

            // %TypedArray%.prototype.map loads the constructor property from the this parameter and uses that to construct the return value.
            // If we set the constructor property of some TypedArray to Array built-in constructor, we should get an array object out.
            var u = getTypedArray();
            u.constructor = Array;
            var r = u.map(mappingFn, thisArg);

            assert.areEqual([0,-1,-2,-3,-4,-5,-6,-7,-8,-9], r, "%TypedArrayPrototype%.map called on a TypedArray with 'constructor' property set to Array produces the correct values");
            assert.isFalse(ArrayBuffer.isView(r), "%TypedArrayPrototype%.map called on a TypedArray with 'constructor' property set to Array produces a normal array");

            // %TypedArray%.prototype.map loads the constructor property from the this parameter and uses that to construct the return value.
            // If we set the constructor property of some TypedArray to a non-constructor, it should throw.
            var u = getTypedArray();
            u.constructor = undefined;
            assert.doesNotThrow(function() { u.map(mappingFn, thisArg); }, "With [@@species], calling %TypedArrayPrototype%.map with a constructor property on this which is not IsConstructor does not throw");

            assert.throws(function() { mapFn.call(); }, TypeError, "Calling %TypedArrayPrototype%.map with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { mapFn.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.map with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { mapFn.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.map with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { mapFn.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.map with no callback function throws TypeError", "[TypedArray].prototype.map: argument is not a Function object");
            assert.throws(function() { mapFn.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.map with undefined callback function throws TypeError", "[TypedArray].prototype.map: argument is not a Function object");
            assert.throws(function() { mapFn.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.map with non-function callback function throws TypeError", "[TypedArray].prototype.map: argument is not a Function object");
        }
    },
    {
        name: "Array.prototype.map called with a TypedArray which lies about length",
        body: function() {
            // We have to make sure that we don't rely on the TypedArray internal length slot when passing a TypedArray object
            // to Array.prototype.map as the this argument. The object might lie about the length property.
            var mapFn = Array.prototype.map;
            var counter = 0;
            var t;
            var thisArg = 'a string';

            function getTypedArray() {
                // Reset the counter var here
                counter = 0;
                // Save the latest array in t for use by the mapping function
                t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function mappingFn(elem, index, arr) {
                assert.areEqual(index, counter, "We call the mapping function on elements in the correct order");
                assert.areEqual(elem, index, "We pass the correct values to the mapping function");
                assert.isTrue(arr === t, "Array passed to the mapping function matches array map is called on");
                assert.areEqual(Object(thisArg), this, "This value passed to map is correct");

                // Increment counter var for the next index
                counter++;

                return -1 * elem;
            }

            // This object lies and says it has length of 5 (while the internal length slot is 10)
            var ui8 = new getTypedArray();
            Object.defineProperty(ui8, 'length', { value: 5 });

            // Since Array.prototype.map doesn't care about TypedArrays and always gets the length property,
            // result array should only have 5 elements.
            var res = mapFn.call(ui8, mappingFn, thisArg);

            assert.areEqual([0,-1,-2,-3,-4], res, "Array.prototype.map called with a TypedArray object which lies about length");
            assert.isFalse(ArrayBuffer.isView(res), "Array.prototype.map returns a normal array object even if the this parameter is a TypedArray");
        }
    },
    {
        name: "%TypedArray%.prototype.forEach behavior",
        body: function() {
            var forEachFn = Int8Array.prototype.__proto__.forEach;
            var counter = 0;
            var t;
            var thisArg = 'a string';
            var verifyThisArg = true;

            function callbackFn(val, k, arr) {
                assert.areEqual(counter, val, "Callback function is called on elements in the correct order");
                assert.areEqual(k, val, "Callback function is called on elements in the correct order");
                assert.isTrue(t === arr, "Callback function is called with the correct TypedArray object");

                if (verifyThisArg) {
                    assert.areEqual(Object(thisArg), this, "Callback function has this set correctly");
                }

                counter++;
            }

            function getTypedArray() {
                // Reset the counter var here
                counter = 0;
                // Save the latest array in t for use by the callback function
                t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            assert.areEqual(undefined, forEachFn.call(getTypedArray(), callbackFn, thisArg), "Calling %TypedArrayPrototype%.forEach with a simple callback function and this arg");
            assert.areEqual(10, counter, "We called the callback function the correct number of times");

            verifyThisArg = false;
            assert.areEqual(undefined, forEachFn.call(getTypedArray(), callbackFn), "Calling %TypedArrayPrototype%.forEach with a simple callback function");
            assert.areEqual(10, counter, "We called the callback function the correct number of times");

            assert.throws(function() { forEachFn.call(); }, TypeError, "Calling %TypedArrayPrototype%.forEach with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { forEachFn.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.forEach with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { forEachFn.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.forEach with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { forEachFn.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.forEach with no callback function throws TypeError", "[TypedArray].prototype.forEach: argument is not a Function object");
            assert.throws(function() { forEachFn.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.forEach with undefined callback function throws TypeError", "[TypedArray].prototype.forEach: argument is not a Function object");
            assert.throws(function() { forEachFn.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.forEach with non-function callback function throws TypeError", "[TypedArray].prototype.forEach: argument is not a Function object");
        }
    },
    {
        name: "%TypedArray%.prototype.indexOf behavior",
        body: function() {
            var indexOfFn = Int8Array.prototype.__proto__.indexOf;

            function getTypedArray() {
                var t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            assert.areEqual(-1, indexOfFn.call(getTypedArray(), undefined), "Calling %TypedArrayPrototype%.indexOf with undefined returns -1");
            assert.areEqual(0, indexOfFn.call(getTypedArray(), 0), "Calling %TypedArrayPrototype%.indexOf searching for the first value");
            assert.areEqual(9, indexOfFn.call(getTypedArray(), 9), "Calling %TypedArrayPrototype%.indexOf searching for the last value");
            assert.areEqual(-1, indexOfFn.call(getTypedArray(), 0, 1), "Calling %TypedArrayPrototype%.indexOf searching for the first value but skipping the first element returns -1");
            assert.areEqual(-1, indexOfFn.call(getTypedArray(), 0, 11), "Calling %TypedArrayPrototype%.indexOf where fromIndex > length returns -1");
            assert.areEqual(0, indexOfFn.call(getTypedArray(), 0, -10), "Calling %TypedArrayPrototype%.indexOf where fromIndex < 0 acts as indexed from the back");
            assert.areEqual(5, indexOfFn.call(getTypedArray(), 5, -5), "Calling %TypedArrayPrototype%.indexOf where fromIndex < 0 acts as indexed from the back");

            // If we use Array.prototype.indexOf but pass TypedArray objects, make sure the property named length is used instead of the internal TypedArray length slot
            var u = getTypedArray();
            Object.defineProperty(u, 'length', { value: 0 });
            assert.areEqual(-1, Array.prototype.indexOf.call(u, 0), "Calling Array.prototype.indexOf where length is zero returns -1");
            var u = getTypedArray();
            Object.defineProperty(u, 'length', { value: 5 });
            assert.areEqual(-1, Array.prototype.indexOf.call(u, 6), "Calling Array.prototype.indexOf with a TypedArray that lies about length - make sure we don't actually find the element");

            assert.throws(function() { indexOfFn.call(); }, TypeError, "Calling %TypedArrayPrototype%.indexOf with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { indexOfFn.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.indexOf with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { indexOfFn.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.indexOf with non-object this throws TypeError", "'this' is not a typed array object");
        }
    },
    {
        name: "%TypedArray%.prototype.includes behavior",
        body: function() {
            var includesFn = Int8Array.prototype.__proto__.includes;

            function getTypedArray() {
                var t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            assert.isFalse(includesFn.call(getTypedArray(), undefined), "Calling %TypedArrayPrototype%.includes with undefined returns true");
            assert.isTrue(includesFn.call(getTypedArray(), 0), "Calling %TypedArrayPrototype%.includes searching for the first value");
            assert.isTrue(includesFn.call(getTypedArray(), 9), "Calling %TypedArrayPrototype%.includes searching for the last value");
            assert.isFalse(includesFn.call(getTypedArray(), 0, 1), "Calling %TypedArrayPrototype%.includes searching for the first value but skipping the first element returns false");
            assert.isFalse(includesFn.call(getTypedArray(), 0, 11), "Calling %TypedArrayPrototype%.includes where fromIndex > length returns false");
            assert.isTrue(includesFn.call(getTypedArray(), 0, -10), "Calling %TypedArrayPrototype%.includes where fromIndex < 0 acts as indexed from the back");
            assert.isTrue(includesFn.call(getTypedArray(), 5, -5), "Calling %TypedArrayPrototype%.includes where fromIndex < 0 acts as indexed from the back");

            // If we use Array.prototype.includes but pass TypedArray objects, make sure the property named length is used instead of the internal TypedArray length slot
            var u = getTypedArray();
            Object.defineProperty(u, 'length', { value: 0 });
            assert.isFalse(Array.prototype.includes.call(u, 0), "Calling Array.prototype.includes where length is zero returns false");
            var u = getTypedArray();
            Object.defineProperty(u, 'length', { value: 5 });
            assert.isFalse(Array.prototype.includes.call(u, 6), "Calling Array.prototype.includes with a TypedArray that lies about length - make sure we don't actually find the element");

            assert.throws(function() { includesFn.call(); }, TypeError, "Calling %TypedArrayPrototype%.includes with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { includesFn.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.includes with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { includesFn.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.includes with non-object this throws TypeError", "'this' is not a typed array object");
        }
    },
    {
        name: "%TypedArray%.prototype.lastIndexOf behavior",
        body: function() {
            var lastIndexOf = Int8Array.prototype.__proto__.lastIndexOf;

            function getTypedArray() {
                var t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            assert.areEqual(-1, lastIndexOf.call(getTypedArray(), undefined), "Calling %TypedArrayPrototype%.lastIndexOf with undefined returns -1");
            assert.areEqual(0, lastIndexOf.call(getTypedArray(), 0), "Calling %TypedArrayPrototype%.lastIndexOf searching for the first value");
            assert.areEqual(9, lastIndexOf.call(getTypedArray(), 9), "Calling %TypedArrayPrototype%.lastIndexOf searching for the last value");
            assert.areEqual(-1, lastIndexOf.call(getTypedArray(), 9, -2), "Calling %TypedArrayPrototype%.lastIndexOf searching for the last value but skipping the last element returns -1");
            assert.areEqual(-1, lastIndexOf.call(getTypedArray(), 4, 2), "Calling %TypedArrayPrototype%.lastIndexOf where fromIndex < the searched element returns -1");
            assert.areEqual(0, lastIndexOf.call(getTypedArray(), 0, -10), "Calling %TypedArrayPrototype%.lastIndexOf where fromIndex < 0 acts as indexed from the back");
            assert.areEqual(5, lastIndexOf.call(getTypedArray(), 5, -5), "Calling %TypedArrayPrototype%.lastIndexOf where fromIndex < 0 acts as indexed from the back");

            var u = getTypedArray();
            u[3] = 2;
            u[4] = 2;
            u[5] = 2;
            assert.areEqual(5, lastIndexOf.call(u, 2), "%TypedArrayPrototype%.lastIndexOf with array that repeats values returns the last index of the value");

            Object.defineProperty(u, 'length', { value: 4 });
            assert.areEqual(3, Array.prototype.lastIndexOf.call(u, 2), "Calling Array.prototype.lastIndexOf where TypedArray contains more instances of search element beyond length");

            var u = getTypedArray();
            Object.defineProperty(u, 'length', { value: 0 });
            assert.areEqual(-1, Array.prototype.lastIndexOf.call(u, 0), "Calling Array.prototype.lastIndexOf where length is zero returns -1");

            assert.throws(function() { lastIndexOf.call(); }, TypeError, "Calling %TypedArrayPrototype%.lastIndexOf with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { lastIndexOf.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.lastIndexOf with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { lastIndexOf.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.lastIndexOf with non-object this throws TypeError", "'this' is not a typed array object");
        }
    },
    {
        name: "%TypedArray%.prototype.reverse behavior",
        body: function() {
            var reverse = Int8Array.prototype.__proto__.reverse;

            function getTypedArray() {
                var t = new Int8Array(10);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            assert.areEqual([9,8,7,6,5,4,3,2,1,0], reverse.call(getTypedArray()), "Calling %TypedArrayPrototype%.reverse with basic behavior");

            var u = getTypedArray();
            assert.areEqual(u, reverse.call(u), "Calling %TypedArrayPrototype%.reverse returns the correct object");
            assert.isTrue(u === reverse.call(u), "Calling %TypedArrayPrototype%.reverse returns the correct object");

            var u = new Uint8Array(3);
            u[0] = 1;
            u[1] = 2;
            u[2] = 3;
            assert.areEqual([3,2,1], reverse.call(u), "Calling %TypedArrayPrototype%.reverse with an odd-length TypedArray");

            var u = new Uint8Array(1);
            u[0] = 1;
            assert.areEqual([1], reverse.call(u), "Calling %TypedArrayPrototype%.reverse with TypedArray of length == 1");

            var u = new Uint8Array(0);
            assert.areEqual([], reverse.call(u), "Calling %TypedArrayPrototype%.reverse with TypedArray of length == 0");

            // Call Array.prototype.reverse passing a TypedArray that lies about length. We should only reverse the part of it less than the indicated length.
            u = getTypedArray();
            Object.defineProperty(u, 'length', { value: 5 });
            assert.areEqual([4,3,2,1,0,5,6,7,8,9], Array.prototype.reverse.call(u), "Calling %TypedArrayPrototype%.reverse with a TypedArray that lies about length");

            // Call Array.prototype.reverse passing a TypedArray that lies about length. TypedArrays do not support delete so we will throw if indicated length is longer than actual.
            u = getTypedArray();
            Object.defineProperty(u, 'length', { value: 20 });
            assert.throws(function() { Array.prototype.reverse.call(u); }, TypeError, "Calling %TypedArrayPrototype%.reverse with a TypedArray that says it has longer length than actual throws", "Object doesn't support this action");

            assert.throws(function() { reverse.call(); }, TypeError, "Calling %TypedArrayPrototype%.reverse with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { reverse.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.reverse with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { reverse.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.reverse with non-object this throws TypeError", "'this' is not a typed array object");
        }
    },
    {
        name: "%TypedArray%.prototype.slice behavior",
        body: function() {
            var slice = Int8Array.prototype.__proto__.slice;

            function getTypedArray(n) {
                var t = new Int8Array(n);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            var u = getTypedArray(10);

            assert.areEqual([0,1,2,3,4,5,6,7,8,9], slice.call(u), "%TypedArrayPrototype%.slice basic functionality");
            assert.areEqual([0,1,2,3,4,5,6,7,8,9], slice.call(u, 0), "%TypedArrayPrototype%.slice basic functionality");
            assert.areEqual([0,1,2,3,4,5,6,7,8,9], slice.call(u, undefined), "%TypedArrayPrototype%.slice basic functionality");
            assert.areEqual([5,6,7,8,9], slice.call(u, 5), "%TypedArrayPrototype%.slice skipping the first 5 elements");
            assert.areEqual([8,9], slice.call(u, -2), "%TypedArrayPrototype%.slice using a negative start index takes only the last elements");
            assert.areEqual([0,1,2,3,4,5,6,7,8,9], slice.call(u, -100), "%TypedArrayPrototype%.slice has the start index clamped to zero");
            assert.areEqual([], slice.call(u, 100), "%TypedArrayPrototype%.slice with start index > length returns empty array");

            assert.areEqual([0,1,2,3,4,5,6,7,8,9], slice.call(u, 0, 50), "%TypedArrayPrototype%.slice has the end index clamped to length");
            assert.areEqual([0,1], slice.call(u, 0, 2), "%TypedArrayPrototype%.slice selecting the first two items");
            assert.areEqual([8,9], slice.call(u, 8, 10), "%TypedArrayPrototype%.slice selecting the last two items");
            assert.areEqual([6,7,8], slice.call(u, 6, -1), "%TypedArrayPrototype%.slice selecting from index 6 until one before the last");
            assert.areEqual([6,7,8], slice.call(u, -4, -1), "%TypedArrayPrototype%.slice selecting from index 6 until one before the last");
            assert.areEqual([5], slice.call(u, 5, 6), "%TypedArrayPrototype%.slice selecting a single item from index 5");

            assert.areEqual([], slice.call(u, 5, 2), "%TypedArrayPrototype%.slice returns empty array if end < start index");
            assert.areEqual([], slice.call(u, 100, 2), "%TypedArrayPrototype%.slice returns empty array if end < start index");
            assert.areEqual([], slice.call(u, 100, -100), "%TypedArrayPrototype%.slice returns empty array if end < start index");

            var r = u.slice();
            assert.isTrue(u !== r, "%TypedArrayPrototype%.slice returns a new object instead of altering the source object");
            assert.isTrue(ArrayBuffer.isView(r), "%TypedArrayPrototype%.slice returns a TypedArray object");
            assert.areEqual(u[Symbol.toStringTag], r[Symbol.toStringTag], "%TypedArrayPrototype%.slice returns the same type of TypedArray as the source object");

            u.constructor = Float32Array;
            var r = u.slice();
            assert.isTrue(ArrayBuffer.isView(r), "%TypedArrayPrototype%.slice returns a TypedArray object");
            assert.areEqual("Float32Array", r[Symbol.toStringTag], "%TypedArrayPrototype%.slice returns the same type of TypedArray as the source object's constructor property");

            u.constructor = String;
            var r = u.slice();
            assert.isTrue(ArrayBuffer.isView(r), "With [@@species], %TypedArrayPrototype%.slice returns a TypedArray object even when source constructor property is not a TypedArray constructor");
            assert.areEqual(0, r[0], "With [@@species], %TypedArrayPrototype%.slice returns a TypedArray object even when source constructor property is String");
            assert.areEqual(1, r[1], "With [@@species], %TypedArrayPrototype%.slice returns a TypedArray object even when source constructor property is String");
            assert.areEqual(2, r[2], "With [@@species], %TypedArrayPrototype%.slice returns a TypedArray object even when source constructor property is String");
            assert.areEqual(3, r[3], "With [@@species], %TypedArrayPrototype%.slice returns a TypedArray object even when source constructor property is String");
            assert.areEqual(8, r[8], "With [@@species], %TypedArrayPrototype%.slice returns a TypedArray object even when source constructor property is String");
            assert.areEqual(9, r[9], "With [@@species], %TypedArrayPrototype%.slice returns a TypedArray object even when source constructor property is String");
            assert.areEqual(10, r.length, "With [@@species], %TypedArrayPrototype%.slice returns a TypedArray object even when source constructor property is String");

            u.constructor = Array;
            assert.throws(function () { u.slice() }, TypeError, "Calling %TypedArrayPrototype%.slice with a constructor property with [@@species] pointing to a non-typed-array constructor throws");

            assert.throws(function() { slice.call(); }, TypeError, "Calling %TypedArrayPrototype%.slice without this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { slice.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.slice with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { slice.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.slice with non-object this throws TypeError", "'this' is not a typed array object");

            u.constructor = Math.sin;
            assert.doesNotThrow(function() { slice.call(u); }, "Calling %TypedArrayPrototype%.slice with constructor property pointing to a non-constructor function can still function through [@@species]");
        }
    },
    {
        name: "%TypedArray%.prototype.sort behavior",
        body: function() {
            var sort = Int8Array.prototype.__proto__.sort;

            function getTypedArray(n) {
                var t = new Int8Array(n);

                for(var i = 0; i < t.length; i++) {
                    t[i] = n-i;
                }

                return t;
            }

            assert.areEqual([1,2,3,4,5,6,7,8,9,10], getTypedArray(10).sort(), "%TypedArrayPrototype%.sort basic behavior");
            assert.areEqual([], getTypedArray(0).sort(), "%TypedArrayPrototype%.sort returns empty array when this arg is empty");
            assert.areEqual([1], getTypedArray(1).sort(), "%TypedArrayPrototype%.sort returns the same array for a single-length array");
            assert.areEqual([1,2], getTypedArray(2).sort(), "%TypedArrayPrototype%.sort returns the correct value for an array with length == 2");

            var u = new Uint8Array(6);
            u[0] = 200;
            u[1] = 150;
            u[2] = u[3] = u[4] = 100;
            u[5] = 0;
            assert.areEqual([0,100,100,100,150,200], u.sort(), "%TypedArrayPrototype%.sort sorts multiple elements with the same value");

            var f = new Float64Array(5);
            f[0] = 100.0;
            f[1] = 99.999999999999;
            f[2] = 99.9999;
            f[3] = 99.99999;
            f[4] = 99.9999;
            assert.areEqual([99.9999,99.9999,99.99999,99.999999999999,100], f.sort(), "%TypedArrayPrototype%.sort basic behavior with 64-bit floats");

            function sortCallbackReverse(x, y) {
                if (x < y) {
                    return 1;
                } else if (x > y) {
                    return -1;
                }

                return 0;
            }

            function sortCallback(x, y) {
                if (x < y) {
                    return -1;
                } else if (x > y) {
                    return 1;
                }

                return 0;
            }

            function sortCallbackHate5(x, y) {
                if (x == 5) {
                    return -1;
                }
                if (y == 5) {
                    return 1;
                }
                return sortCallback(x, y);
            }

            function sortCallbackMalformed(x, y) {
                switch(x) {
                case 1:
                    return 0;
                case 2:
                    return -10.5;
                case 3:
                    return 'a string';
                case 4:
                    return { foo: 'another string' };
                case 5:
                    return function foo() { return 90; };
                case 6:
                    return 12.99999999999;
                }

                return undefined;
            }

            assert.areEqual([1,2,3,4,5,6,7,8,9,10], getTypedArray(10).sort(sortCallback), "%TypedArrayPrototype%.sort basic behavior with a non-lying sort callback");
            assert.areEqual([10,9,8,7,6,5,4,3,2,1], getTypedArray(10).sort(sortCallbackReverse), "%TypedArrayPrototype%.sort with a sort callback function which reverses elements");
            assert.areEqual([5,1,2,3,4,6,7,8,9,10], getTypedArray(10).sort(sortCallbackHate5), "%TypedArrayPrototype%.sort basic behavior with a lying sort callback which hates the number 5");
            assert.areEqual([9,8,7,2,10,5,4,3,1,6], getTypedArray(10).sort(sortCallbackMalformed), "%TypedArrayPrototype%.sort basic behavior with a sort callback which returns random values");

            assert.throws(function() { sort.call(); }, TypeError, "Calling %TypedArrayPrototype%.sort with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { sort.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.sort with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { sort.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.sort with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { sort.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.sort with non-function callback function throws TypeError", "[TypedArray].prototype.sort: argument is not a Function object");
        }
    },
    {
        name: "%TypedArray%.prototype.find behavior",
        body: function() {
            var findFn = Int8Array.prototype.__proto__.find;
            var thisArg = 'value';
            var t;
            var counter;

            function getTypedArray(n) {
                // Also remember t for use in verifying in the callback
                t = new Int8Array(n);

                // Reset counter
                counter = 0;

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function assertCallbackBehavior(val, k, arr, _that) {
                assert.areEqual(counter, k, "%TypedArrayPrototype%.find callback called on elements in order");
                assert.areEqual(val, k, "%TypedArrayPrototype%.find callback called with correct values");
                assert.areEqual(Object(thisArg), _that, "%TypedArrayPrototype%.find callback function should receive the correct this arg");
                assert.areEqual(t, arr, "%TypedArrayPrototype%.find callback called with the correct array arg");
            }

            var expectedValue = 5;
            function callbackFalseOnExpectedValue(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                if (expectedValue === val) {
                    return true;
                }

                return false;
            }

            function callbackAlwaysFalse(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                return false;
            }

            assert.areEqual(undefined, findFn.call(getTypedArray(10), callbackAlwaysFalse, thisArg), "%TypedArrayPrototype%.find returns undefined when the callback always returns false");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.find calls the callback function the correct number of times");

            assert.areEqual(expectedValue, findFn.call(getTypedArray(10), callbackFalseOnExpectedValue, thisArg), "%TypedArrayPrototype%.find returns the value of the array which causes the callback to return true");
            assert.areEqual(6, counter, "%TypedArrayPrototype%.find calls the callback function the correct number of times");

            expectedValue = 0;
            assert.areEqual(expectedValue, findFn.call(getTypedArray(10), callbackFalseOnExpectedValue, thisArg), "%TypedArrayPrototype%.find returns the value of the array which causes the callback to return true");
            assert.areEqual(1, counter, "%TypedArrayPrototype%.find calls the callback function the correct number of times");

            expectedValue = 9;
            assert.areEqual(expectedValue, findFn.call(getTypedArray(10), callbackFalseOnExpectedValue, thisArg), "%TypedArrayPrototype%.find returns the value of the array which causes the callback to return true");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.find calls the callback function the correct number of times");

            assert.throws(function() { findFn.call(); }, TypeError, "Calling %TypedArrayPrototype%.find with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { findFn.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.find with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { findFn.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.find with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { findFn.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.find with no callback function throws TypeError", "[TypedArray].prototype.find: argument is not a Function object");
            assert.throws(function() { findFn.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.find with undefined callback function throws TypeError", "[TypedArray].prototype.find: argument is not a Function object");
            assert.throws(function() { findFn.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.find with non-function callback function throws TypeError", "[TypedArray].prototype.find: argument is not a Function object");
        }
    },
    {
        name: "%TypedArray%.prototype.findIndex behavior",
        body: function() {
            var findIndex = Int8Array.prototype.__proto__.findIndex;
            var thisArg = 'value';
            var t;
            var counter;

            function getTypedArray(n) {
                // Also remember t for use in verifying in the callback
                t = new Int8Array(n);

                // Reset counter
                counter = 0;

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function assertCallbackBehavior(val, k, arr, _that) {
                assert.areEqual(counter, k, "%TypedArrayPrototype%.findIndex callback called on elements in order");
                assert.areEqual(val, k, "%TypedArrayPrototype%.findIndex callback called with correct values");
                assert.areEqual(Object(thisArg), _that, "%TypedArrayPrototype%.findIndex callback function should receive the correct this arg");
                assert.areEqual(t, arr, "%TypedArrayPrototype%.findIndex callback called with the correct array arg");
            }

            var expectedValue = 5;
            function callbackFalseOnExpectedValue(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                if (expectedValue === val) {
                    return true;
                }

                return false;
            }

            function callbackAlwaysFalse(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                return false;
            }

            assert.areEqual(-1, findIndex.call(getTypedArray(10), callbackAlwaysFalse, thisArg), "%TypedArrayPrototype%.findIndex returns -1 when the callback always returns false");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.findIndex calls the callback function the correct number of times");

            assert.areEqual(expectedValue, findIndex.call(getTypedArray(10), callbackFalseOnExpectedValue, thisArg), "%TypedArrayPrototype%.findIndex returns the index of the element of the array which causes the callback to return true");
            assert.areEqual(6, counter, "%TypedArrayPrototype%.findIndex calls the callback function the correct number of times");

            expectedValue = 0;
            assert.areEqual(expectedValue, findIndex.call(getTypedArray(10), callbackFalseOnExpectedValue, thisArg), "%TypedArrayPrototype%.findIndex returns the index of the element of the array which causes the callback to return true");
            assert.areEqual(1, counter, "%TypedArrayPrototype%.findIndex calls the callback function the correct number of times");

            expectedValue = 9;
            assert.areEqual(expectedValue, findIndex.call(getTypedArray(10), callbackFalseOnExpectedValue, thisArg), "%TypedArrayPrototype%.findIndex returns the index of the element of the array which causes the callback to return true");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.findIndex calls the callback function the correct number of times");

            assert.throws(function() { findIndex.call(); }, TypeError, "Calling %TypedArrayPrototype%.findIndex with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { findIndex.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.findIndex with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { findIndex.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.findIndex with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { findIndex.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.findIndex with no callback function throws TypeError", "[TypedArray].prototype.findIndex: argument is not a Function object");
            assert.throws(function() { findIndex.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.findIndex with undefined callback function throws TypeError", "[TypedArray].prototype.findIndex: argument is not a Function object");
            assert.throws(function() { findIndex.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.findIndex with non-function callback function throws TypeError", "[TypedArray].prototype.findIndex: argument is not a Function object");
        }
    },
    {
        name: "%TypedArray%.prototype.filter behavior",
        body: function() {
            var filter = Int8Array.prototype.__proto__.filter;
            var thisArg = 'value';
            var t;
            var counter;

            function getTypedArray(n) {
                // Also remember t for use in verifying in the callback
                t = new Int8Array(n);

                // Reset counter
                counter = 0;

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function assertCallbackBehavior(val, k, arr, _that) {
                assert.areEqual(counter, k, "%TypedArrayPrototype%.filter callback called on elements in order");
                assert.areEqual(val, k, "%TypedArrayPrototype%.filter callback called with correct values");
                assert.areEqual(Object(thisArg), _that, "%TypedArrayPrototype%.filter callback function should receive the correct this arg");
                assert.areEqual(t, arr, "%TypedArrayPrototype%.filter callback called with the correct array arg");
            }

            function selectOddNumbers(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                if (val % 2 != 0) {
                    return true;
                }

                return false;
            }

            function selectNone(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                return false;
            }

            function selectAll(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                return true;
            }

            var counter = 0;
            var res = filter.call(getTypedArray(10), selectOddNumbers, thisArg);
            assert.areEqual([1,3,5,7,9], res, "%TypedArrayPrototype%.filter returns a new TypedArray with the right values");
            assert.isTrue(ArrayBuffer.isView(res), "%TypedArrayPrototype%.filter returns a new TypedArray");
            assert.areEqual("Int8Array", res[Symbol.toStringTag], "%TypedArrayPrototype%.filter returns the right type of new TypedArray");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.filter calls the callback function the correct number of times");

            assert.areEqual([], filter.call(getTypedArray(10), selectNone, thisArg), "%TypedArrayPrototype%.filter returns an empty array when callback selects no elements");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.filter calls the callback function the correct number of times");

            assert.areEqual([0,1,2,3,4,5,6,7,8,9], filter.call(getTypedArray(10), selectAll, thisArg), "%TypedArrayPrototype%.filter returns the original array when callback selects all elements");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.filter calls the callback function the correct number of times");

            var u = getTypedArray(10);
            u.constructor = Array;
            counter = 0;
            var res = filter.call(u, selectOddNumbers, thisArg);
            assert.areEqual([1,3,5,7,9], res, "%TypedArrayPrototype%.filter returns a new array with the right values");
            assert.isFalse(ArrayBuffer.isView(res), "%TypedArrayPrototype%.filter returns a normal array if the TypedArray constructor property is changed");
            assert.areEqual(undefined, res[Symbol.toStringTag], "%TypedArrayPrototype%.filter return value doesn't have @@toStringTag value");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.filter calls the callback function the correct number of times");

            u.constructor = Math.sin;
            counter = 0;
            assert.doesNotThrow(function() { filter.call(u, selectOddNumbers, thisArg); }, "Calling %TypedArrayPrototype%.filter with constructor property pointing to a non-constructor function can still function through [@@species]");

            assert.throws(function() { filter.call(); }, TypeError, "Calling %TypedArrayPrototype%.filter with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { filter.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.filter with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { filter.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.filter with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { filter.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.filter with no callback function throws TypeError", "[TypedArray].prototype.filter: argument is not a Function object");
            assert.throws(function() { filter.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.filter with undefined callback function throws TypeError", "[TypedArray].prototype.filter: argument is not a Function object");
            assert.throws(function() { filter.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.filter with non-function callback function throws TypeError", "[TypedArray].prototype.filter: argument is not a Function object");
        }
    },
    {
        name: "%TypedArray%.prototype.every behavior",
        body: function() {
            var every = Int8Array.prototype.__proto__.every;
            var thisArg = 'value';
            var t;
            var counter;

            function getTypedArray(n) {
                // Also remember t for use in verifying in the callback
                t = new Int8Array(n);

                // Reset counter
                counter = 0;

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function assertCallbackBehavior(val, k, arr, _that) {
                assert.areEqual(counter, k, "%TypedArrayPrototype%.every callback called on elements in order");
                assert.areEqual(val, k, "%TypedArrayPrototype%.every callback called with correct values");
                assert.areEqual(Object(thisArg), _that, "%TypedArrayPrototype%.every callback function should receive the correct this arg");
                assert.areEqual(t, arr, "%TypedArrayPrototype%.every callback called with the correct array arg");
            }

            var expectedValue = 5;
            function callbackFalseOnExpectedValue(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                if (expectedValue === val) {
                    return false;
                }

                return true;
            }

            function callbackAlwaysTrue(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                return true;
            }

            assert.isTrue(every.call(getTypedArray(10), callbackAlwaysTrue, thisArg), "%TypedArrayPrototype%.every returns true when the callback always returns true");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.every calls the callback function the correct number of times");

            assert.isFalse(every.call(getTypedArray(10), callbackFalseOnExpectedValue, thisArg), "%TypedArrayPrototype%.every returns false if the callback ever returns false");
            assert.areEqual(6, counter, "%TypedArrayPrototype%.every calls the callback function the correct number of times");

            assert.throws(function() { every.call(); }, TypeError, "Calling %TypedArrayPrototype%.every with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { every.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.every with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { every.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.every with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { every.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.every with no callback function throws TypeError", "[TypedArray].prototype.every: argument is not a Function object");
            assert.throws(function() { every.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.every with undefined callback function throws TypeError", "[TypedArray].prototype.every: argument is not a Function object");
            assert.throws(function() { every.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.every with non-function callback function throws TypeError", "[TypedArray].prototype.every: argument is not a Function object");
        }
    },
    {
        name: "%TypedArray%.prototype.some behavior",
        body: function() {
            var some = Int8Array.prototype.__proto__.some;
            var thisArg = 'value';
            var t;
            var counter;

            function getTypedArray(n) {
                // Also remember t for use in verifying in the callback
                t = new Int8Array(n);

                // Reset counter
                counter = 0;

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function assertCallbackBehavior(val, k, arr, _that) {
                assert.areEqual(counter, k, "%TypedArrayPrototype%.some callback called on elements in order");
                assert.areEqual(val, k, "%TypedArrayPrototype%.some callback called with correct values");
                assert.areEqual(Object(thisArg), _that, "%TypedArrayPrototype%.some callback function should receive the correct this arg");
                assert.areEqual(t, arr, "%TypedArrayPrototype%.some callback called with the correct array arg");
            }

            var expectedValue = 5;
            function callbackTrueOnExpectedValue(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                if (expectedValue === val) {
                    return true;
                }

                return false;
            }

            function callbackAlwaysFalse(val, k, arr) {
                assertCallbackBehavior(val, k, arr, this);

                counter++;

                return false;
            }

            assert.isFalse(some.call(getTypedArray(10), callbackAlwaysFalse, thisArg), "%TypedArrayPrototype%.some returns false when the callback always returns false");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.some calls the callback function the correct number of times");

            assert.isTrue(some.call(getTypedArray(10), callbackTrueOnExpectedValue, thisArg), "%TypedArrayPrototype%.some returns true if the callback ever returns true");
            assert.areEqual(6, counter, "%TypedArrayPrototype%.some calls the callback function the correct number of times");

            assert.throws(function() { some.call(); }, TypeError, "Calling %TypedArrayPrototype%.some with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { some.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.some with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { some.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.some with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { some.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.some with no callback function throws TypeError", "[TypedArray].prototype.some: argument is not a Function object");
            assert.throws(function() { some.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.some with undefined callback function throws TypeError", "[TypedArray].prototype.some: argument is not a Function object");
            assert.throws(function() { some.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.some with non-function callback function throws TypeError", "[TypedArray].prototype.some: argument is not a Function object");
        }
    },
    {
        name: "%TypedArray%.prototype.join behavior",
        body: function() {
            var join = Int8Array.prototype.__proto__.join;

            function getTypedArray(n) {
                var t = new Int8Array(n);

                for(var i = 0; i < t.length; i++) {
                    t[i] = i+1;
                }

                return t;
            }

            assert.areEqual('1,2,3,4,5,6,7,8,9,10', join.call(getTypedArray(10)), "%TypedArrayPrototype%.join basic behavior");
            assert.areEqual('', join.call(getTypedArray(0)), "%TypedArrayPrototype%.join called with zero-length TypedArray");
            assert.areEqual('1', join.call(getTypedArray(1)), "%TypedArrayPrototype%.join called with single-length TypedArray");
            assert.areEqual('1,2', join.call(getTypedArray(2)), "%TypedArrayPrototype%.join called with length == 2 TypedArray");
            assert.areEqual('1,2,3', join.call(getTypedArray(3)), "%TypedArrayPrototype%.join called with length == 3 TypedArray");

            assert.throws(function() { join.call(); }, TypeError, "Calling %TypedArrayPrototype%.join with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { join.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.join with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { join.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.join with non-object this throws TypeError", "'this' is not a typed array object");
        }
    },
    {
        name: "%TypedArray%.prototype.reduce behavior",
        body: function() {
            var reduce = Int8Array.prototype.__proto__.reduce;
            var t;
            var counter;
            var thisArg = Object(this);

            function getTypedArray(n) {
                t = new Int8Array(n);
                counter = 0;

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function sumItemsCallback(accumulator, val, k, arr) {
                assert.areEqual(counter, k, "%TypedArrayPrototype%.reduce calls the callback on items in order");
                assert.areEqual(val, k, "%TypedArrayPrototype%.reduce calls the callback with the correct values");
                assert.areEqual(thisArg, this, "%TypedArrayPrototype%.reduce calls the callback with undefined as the this arg");
                assert.areEqual(t, arr, "%TypedArrayPrototype%.reduce calls the callback with the correct array object");

                counter++;

                return accumulator + val;
            }

            assert.areEqual(45, reduce.call(getTypedArray(10), sumItemsCallback, 0), "%TypedArrayPrototype%.reduce basic functionality");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.reduce called the callback the correct number of times");
            assert.areEqual(100, reduce.call(getTypedArray(0), sumItemsCallback, 100), "%TypedArrayPrototype%.reduce with a zero-length array returns the initial value");
            assert.areEqual(0, counter, "%TypedArrayPrototype%.reduce didn't call the callback on an empty array");

            var u = getTypedArray(10);
            counter = 1;
            assert.areEqual(45, reduce.call(u, sumItemsCallback), "%TypedArrayPrototype%.reduce called with no initial value causes the first index passed to the callback to be shifted by one");
            assert.areEqual(10, counter, "%TypedArrayPrototype%.reduce called the callback the correct number of times");

            assert.throws(function() { reduce.call(); }, TypeError, "Calling %TypedArrayPrototype%.reduce with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { reduce.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.reduce with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { reduce.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.reduce with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { reduce.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.reduce with no callback function throws TypeError", "[TypedArray].prototype.reduce: argument is not a Function object");
            assert.throws(function() { reduce.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.reduce with undefined callback function throws TypeError", "[TypedArray].prototype.reduce: argument is not a Function object");
            assert.throws(function() { reduce.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.reduce with non-function callback function throws TypeError", "[TypedArray].prototype.reduce: argument is not a Function object");
            assert.throws(function() { reduce.call(getTypedArray(0), sumItemsCallback); }, TypeError, "%TypedArrayPrototype%.reduce throws if array has zero length");
        }
    },
    {
        name: "%TypedArray%.prototype.reduceRight behavior",
        body: function() {
            var reduceRight = Int8Array.prototype.__proto__.reduceRight;
            var t;
            var counter;
            var thisArg = Object(this);

            function getTypedArray(n) {
                t = new Int8Array(n);

                // counter starts at the end for reduceRight (last index == n-1)
                counter = n-1;

                for(var i = 0; i < t.length; i++) {
                    t[i] = i;
                }

                return t;
            }

            function sumItemsCallback(accumulator, val, k, arr) {
                assert.areEqual(counter, k, "%TypedArrayPrototype%.reduceRight calls the callback on items in order");
                assert.areEqual(val, k, "%TypedArrayPrototype%.reduceRight calls the callback with the correct values");
                assert.areEqual(thisArg, this, "%TypedArrayPrototype%.reduceRight calls the callback with undefined as the this arg");
                assert.areEqual(t, arr, "%TypedArrayPrototype%.reduceRight calls the callback with the correct array object");

                counter--;

                return accumulator + val;
            }

            assert.areEqual(45, reduceRight.call(getTypedArray(10), sumItemsCallback, 0), "%TypedArrayPrototype%.reduceRight basic functionality");
            assert.areEqual(-1, counter, "%TypedArrayPrototype%.reduceRight called the callback the correct number of times");
            assert.areEqual(100, reduceRight.call(getTypedArray(0), sumItemsCallback, 100), "%TypedArrayPrototype%.reduceRight with a zero-length array returns the initial value");
            assert.areEqual(-1, counter, "%TypedArrayPrototype%.reduceRight didn't call the callback on an empty array");

            var u = getTypedArray(10);
            counter = 8; // second-to-last index
            assert.areEqual(45, reduceRight.call(u, sumItemsCallback), "%TypedArrayPrototype%.reduceRight called with no initial value causes the first index passed to the callback to be shifted by one");
            assert.areEqual(-1, counter, "%TypedArrayPrototype%.reduceRight called the callback the correct number of times");

            assert.throws(function() { reduceRight.call(); }, TypeError, "Calling %TypedArrayPrototype%.reduceRight with no this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { reduceRight.call(undefined); }, TypeError, "Calling %TypedArrayPrototype%.reduceRight with undefined this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { reduceRight.call('string'); }, TypeError, "Calling %TypedArrayPrototype%.reduceRight with non-object this throws TypeError", "'this' is not a typed array object");
            assert.throws(function() { reduceRight.call(new Uint8Array(10)); }, TypeError, "Calling %TypedArrayPrototype%.reduceRight with no callback function throws TypeError", "[TypedArray].prototype.reduceRight: argument is not a Function object");
            assert.throws(function() { reduceRight.call(new Uint8Array(10), undefined); }, TypeError, "Calling %TypedArrayPrototype%.reduceRight with undefined callback function throws TypeError", "[TypedArray].prototype.reduceRight: argument is not a Function object");
            assert.throws(function() { reduceRight.call(new Uint8Array(10), 'string'); }, TypeError, "Calling %TypedArrayPrototype%.reduceRight with non-function callback function throws TypeError", "[TypedArray].prototype.reduceRight: argument is not a Function object");
            assert.throws(function() { reduceRight.call(getTypedArray(0), sumItemsCallback); }, TypeError, "%TypedArrayPrototype%.reduceRight throws if array has zero length");
        }
    },
    {
        name: "ArrayBuffer.isView API shape verification",
        body: function() {
            assert.isFalse(ArrayBuffer.isView === undefined, "ArrayBuffer.isView !== undefined");
            assert.areEqual('function', typeof ArrayBuffer.isView, "typeof ArrayBuffer.isView == 'function'");
            assert.areEqual(1, ArrayBuffer.isView.length, "ArrayBuffer.isView.length == 1");

            var descriptor = Object.getOwnPropertyDescriptor(ArrayBuffer, 'isView');

            assert.isTrue(descriptor.writable, "ArrayBuffer.isView.writable === true");
            assert.isFalse(descriptor.enumerable, "ArrayBuffer.isView.enumerable === false");
            assert.isTrue(descriptor.configurable, "ArrayBuffer.isView.configurable === true");
        }
    },
    {
        name: "ArrayBuffer.isView behavior",
        body: function() {
            // All of the TypedArray prototypes should be ordinary objects and should return false when passed to ArrayBuffer.isView.
            assert.isFalse(ArrayBuffer.isView(ArrayBuffer.prototype), "ArrayBuffer.isView(ArrayBuffer.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(DataView.prototype), "ArrayBuffer.isView(DataView.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Uint8Array.prototype), "ArrayBuffer.isView(Uint8Array.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Uint16Array.prototype), "ArrayBuffer.isView(Uint16Array.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Uint32Array.prototype), "ArrayBuffer.isView(Uint32Array.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Int8Array.prototype), "ArrayBuffer.isView(Int8Array.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Int16Array.prototype), "ArrayBuffer.isView(Int16Array.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Int32Array.prototype), "ArrayBuffer.isView(Int32Array.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Uint8ClampedArray.prototype), "ArrayBuffer.isView(Uint8ClampedArray.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Float32Array.prototype), "ArrayBuffer.isView(Float32Array.prototype) === false");
            assert.isFalse(ArrayBuffer.isView(Float64Array.prototype), "ArrayBuffer.isView(Float64Array.prototype) === false");

            assert.isTrue(ArrayBuffer.isView(new DataView(new ArrayBuffer(20))), "ArrayBuffer.isView(new DataView(new ArrayBuffer(20))) === true");
            assert.isTrue(ArrayBuffer.isView(new Uint8Array(10)), "ArrayBuffer.isView(new Uint8Array(10)) === true");
            assert.isTrue(ArrayBuffer.isView(new Uint16Array(10)), "ArrayBuffer.isView(new Uint16Array(10)) === true");
            assert.isTrue(ArrayBuffer.isView(new Uint32Array(10)), "ArrayBuffer.isView(new Uint32Array(10)) === true");
            assert.isTrue(ArrayBuffer.isView(new Int8Array(10)), "ArrayBuffer.isView(new Int8Array(10)) === true");
            assert.isTrue(ArrayBuffer.isView(new Int16Array(10)), "ArrayBuffer.isView(new Int16Array(10)) === true");
            assert.isTrue(ArrayBuffer.isView(new Int32Array(10)), "ArrayBuffer.isView(new Int32Array(10)) === true");
            assert.isTrue(ArrayBuffer.isView(new Uint8ClampedArray(10)), "ArrayBuffer.isView(new Uint8ClampedArray(10)) === true");
            assert.isTrue(ArrayBuffer.isView(new Float32Array(10)), "ArrayBuffer.isView(new Float32Array(10)) === true");
            assert.isTrue(ArrayBuffer.isView(new Float64Array(10)), "ArrayBuffer.isView(new Float64Array(10)) === true");

            assert.isFalse(ArrayBuffer.isView(undefined), "ArrayBuffer.isView(undefined) === false");
            assert.isFalse(ArrayBuffer.isView(null), "ArrayBuffer.isView(null) === false");
            assert.isFalse(ArrayBuffer.isView(), "ArrayBuffer.isView() === false");
            assert.isFalse(ArrayBuffer.isView(new ArrayBuffer(20)), "ArrayBuffer.isView(new ArrayBuffer(20)) === false");
            assert.isFalse(ArrayBuffer.isView({buffer:[0,1,2],byteLength:12,byteOffset:0}), "ArrayBuffer.isView({buffer:[0,1,2],byteLength:12,byteOffset:0}) === false");
            assert.isFalse(ArrayBuffer.isView([1,2,3]), "ArrayBuffer.isView([1,2,3]) === false");
            assert.isFalse(ArrayBuffer.isView(true), "ArrayBuffer.isView(true) === false");
            assert.isFalse(ArrayBuffer.isView(false), "ArrayBuffer.isView(false) === false");
            assert.isFalse(ArrayBuffer.isView(0), "ArrayBuffer.isView(0) === false");
            assert.isFalse(ArrayBuffer.isView(1), "ArrayBuffer.isView(1) === false");
            assert.isFalse(ArrayBuffer.isView(1.0), "ArrayBuffer.isView(1.0) === false");
            assert.isFalse(ArrayBuffer.isView('string'), "ArrayBuffer.isView('string') === false");
            assert.isFalse(ArrayBuffer.isView({}), "ArrayBuffer.isView({}) === false");
            assert.isFalse(ArrayBuffer.isView(function() {}), "ArrayBuffer.isView(function() {}) === false");
            assert.isFalse(ArrayBuffer.isView(new Array(1)), "ArrayBuffer.isView(new Array(1)) === false");
        }
    },
    {
        name: "ArrayBuffer.prototype.slice API shape verification",
        body: function() {
            assert.isFalse(ArrayBuffer.prototype.slice === undefined, "ArrayBuffer.prototype.slice !== undefined");
            assert.areEqual('function', typeof ArrayBuffer.prototype.slice, "typeof ArrayBuffer.prototype.slice == 'function'");
            assert.areEqual(2, ArrayBuffer.prototype.slice.length, "ArrayBuffer.prototype.slice.length == 2");
        }
    },
    {
        name: "ArrayBuffer.prototype.slice behavior with non-ArrayBuffer parameters",
        body: function() {
            assert.throws(function () { ArrayBuffer.prototype.slice.apply('string'); }, TypeError, "ArrayBuffer.prototype.slice throws TypeError if this parameter is not an ArrayBuffer", "ArrayBuffer object expected");
            assert.throws(function () { ArrayBuffer.prototype.slice.apply(); }, TypeError, "ArrayBuffer.prototype.slice throws TypeError if there is no this parameter", "ArrayBuffer object expected");
            assert.throws(function () { ArrayBuffer.prototype.slice.call(); }, TypeError, "ArrayBuffer.prototype.slice throws TypeError if it is called directly", "ArrayBuffer object expected");
            assert.throws(function () { ArrayBuffer.prototype.slice.call(undefined); }, TypeError, "ArrayBuffer.prototype.slice throws TypeError if it is called directly", "ArrayBuffer object expected");
            assert.throws(function () { ArrayBuffer.prototype.slice(); }, TypeError, "ArrayBuffer.prototype.slice throws TypeError if it is called directly", "ArrayBuffer object expected");
            assert.throws(function () { ArrayBuffer.prototype.slice(undefined); }, TypeError, "ArrayBuffer.prototype.slice throws TypeError if it is called directly", "ArrayBuffer object expected");
        }
    },
    {
        name: "ArrayBuffer.prototype.slice behavior with positive start offset",
        body: function() {
            var len = 10;
            var buf = new ArrayBuffer(len);
            var u8 = new Uint8Array(buf);

            for (var i = 0; i < u8.length; i++)
            {
                u8[i] = i + 1;
            }

            var sliced = buf.slice();
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice() returns the same array elements as the original");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice() returns an ArrayBuffer with the same byteLength as the original");

            var sliced = buf.slice(0);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice(0) returns the same array elements as the original");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice(0) returns an ArrayBuffer with the same byteLength as the original");

            var sliced = buf.slice(5);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([6,7,8,9,10], slicedArray, "slice(5) contains the last 5 elements - [6,7,8,9,10]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(5, sliced.byteLength, "slice(5) returns an ArrayBuffer with byteLength = 5");

            var sliced = buf.slice(9);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([10], slicedArray, "slice(9) contains the last 1 elements - [10]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(1, sliced.byteLength, "slice(9) returns an ArrayBuffer with byteLength = 1");

            var sliced = buf.slice(10);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([], slicedArray, "slice(10) contains empty buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(0, sliced.byteLength, "slice(10) returns an ArrayBuffer with byteLength = 0");

            var sliced = buf.slice(15);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([], slicedArray, "slice(15) contains empty buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(0, sliced.byteLength, "slice(15) returns an ArrayBuffer with byteLength = 0");

            var sliced = buf.slice(100);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([], slicedArray, "slice(100) contains empty buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(0, sliced.byteLength, "slice(100) returns an ArrayBuffer with byteLength = 0");
        }
    },
    {
        name: "ArrayBuffer.prototype.slice behavior with negative start offset",
        body: function() {
            var len = 10;
            var buf = new ArrayBuffer(len);
            var u8 = new Uint8Array(buf);

            for (var i = 0; i < u8.length; i++)
            {
                u8[i] = i + 1;
            }

            var sliced = buf.slice(-4);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([7,8,9,10], slicedArray, "slice(-4) returns the last 4 elements - [7,8,9,10]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(4, sliced.byteLength, "slice(-4) returns an ArrayBuffer with the byteLength = 4");

            var sliced = buf.slice(-5);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([6,7,8,9,10], slicedArray, "slice(-5) contains the last 5 elements - [6,7,8,9,10]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(5, sliced.byteLength, "slice(-5) returns an ArrayBuffer with byteLength = 5");

            var sliced = buf.slice(-9);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([2,3,4,5,6,7,8,9,10], slicedArray, "slice(-9) contains the last 9 elements - [2,3,4,5,6,7,8,9,10]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(9, sliced.byteLength, "slice(-9) returns an ArrayBuffer with byteLength = 9");

            var sliced = buf.slice(-10);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice(-10) returns ArrayBuffer containing the entire original buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice(-10) returns an ArrayBuffer with the same byteLength as the original");

            var sliced = buf.slice(-15);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice(-15) returns ArrayBuffer containing the entire original buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice(-15) returns an ArrayBuffer with the same byteLength as the original");

            var sliced = buf.slice(-100);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice(-100) returns ArrayBuffer containing the entire original buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice(-100) returns an ArrayBuffer with the same byteLength as the original");
        }
    },
    {
        name: "ArrayBuffer.prototype.slice behavior with positive start offset and positive end offset",
        body: function() {
            var len = 10;
            var buf = new ArrayBuffer(len);
            var u8 = new Uint8Array(buf);

            for (var i = 0; i < u8.length; i++)
            {
                u8[i] = i + 1;
            }

            var sliced = buf.slice(0, len);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice(0, len) returns all the elements of the original buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice(0, len) returns an ArrayBuffer with the same byteLength as the original");

            var sliced = buf.slice(0, len * 10);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice(0, len*10) returns all the elements of the original buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice(0, len*10) returns an ArrayBuffer with the same byteLength as the original");

            var sliced = buf.slice(0, 5);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([1,2,3,4,5], slicedArray, "slice(0, 5) contains the first 5 elements - [1,2,3,4,5]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(5, sliced.byteLength, "slice(0, 5) returns an ArrayBuffer with byteLength = 5");

            var sliced = buf.slice(1, 1);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([], slicedArray, "slice(1, 1) contains no elements - the empty buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(0, sliced.byteLength, "slice(1, 1) returns an ArrayBuffer with byteLength = 0");

            var sliced = buf.slice(5, 10);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([6,7,8,9,10], slicedArray, "slice(5, 10) returns the last 5 elements - [6,7,8,9,10]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(5, sliced.byteLength, "slice(5, 10) returns an ArrayBuffer with byteLength = 5");

            var sliced = buf.slice(9, 10);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([10], slicedArray, "slice(9, 10) returns the last element - [10]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(1, sliced.byteLength, "slice(9, 10) returns an ArrayBuffer with byteLength = 1");

            var sliced = buf.slice(7, 5);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([], slicedArray, "slice(7, 5) returns empty ArrayBuffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(0, sliced.byteLength, "slice(7, 5) returns an ArrayBuffer with byteLength = 0");
        }
    },
    {
        name: "ArrayBuffer.prototype.slice behavior with mix of positive and negative start offset and end offset",
        body: function() {
            var len = 10;
            var buf = new ArrayBuffer(len);
            var u8 = new Uint8Array(buf);

            for (var i = 0; i < u8.length; i++)
            {
                u8[i] = i + 1;
            }

            var sliced = buf.slice(5, -2);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([6,7,8], slicedArray, "slice(5, -2) returns elements 5 through (len-2) = [6,7,8]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(3, sliced.byteLength, "slice(5, -2) returns an ArrayBuffer with byteLength = 3");

            var sliced = buf.slice(-5, 8);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([6,7,8], slicedArray, "slice(-5, 8) returns elements 5 through 8 = [6,7,8]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(3, sliced.byteLength, "slice(-5, 8) returns an ArrayBuffer with byteLength = 3");

            var sliced = buf.slice(-10, buf.byteLength);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice(-10, len) returns elements all the elements of the original buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice(-10, len) returns an ArrayBuffer the same byteLength as the original");

            var sliced = buf.slice(-20, buf.byteLength * 2);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual(u8, slicedArray, "slice(-20, len*2) returns elements all the elements of the original buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(buf.byteLength, sliced.byteLength, "slice(-20, len*2) returns an ArrayBuffer the same byteLength as the original");

            var sliced = buf.slice(-7, -3);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([4,5,6,7], slicedArray, "slice(-7, -3) returns elements 3 through (len-3) = [4,5,6,7]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(4, sliced.byteLength, "slice(-7, -3) returns an ArrayBuffer with byteLength = 4");

            var sliced = buf.slice(-3, -7);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([], slicedArray, "slice(-3, -7) returns empty buffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(0, sliced.byteLength, "slice(-3, -7) returns an ArrayBuffer with byteLength = 0");
        }
    },
    {
        name: "DataView.prototype.buffer API shape",
        body: function() {
            assert.isTrue(DataView.prototype.hasOwnProperty('buffer'), "ArrayBuffer.prototype.hasOwnProperty('buffer') == true");

            descriptor = Object.getOwnPropertyDescriptor(DataView.prototype, 'buffer');

            assert.isTrue(descriptor !== undefined, "Object.getOwnPropertyDescriptor(DataView.prototype, 'buffer') !== undefined");

            assert.isTrue(descriptor.get !== undefined, "DataView.prototype.buffer.get !== undefined");
            assert.isTrue(typeof descriptor.get === 'function', "typeof DataView.prototype.buffer.get === 'function'");
            assert.areEqual(0, descriptor.get.length, "DataView.prototype.buffer.get.length === 0");

            assert.isTrue(descriptor.set === undefined, "DataView.prototype.buffer.set === undefined");

            assert.isFalse(descriptor.enumerable, "DataView.prototype.buffer.enumerable === false");
            assert.isTrue(descriptor.configurable, "DataView.prototype.buffer.configurable === true");
        }
    },
    {
        name: "DataView.prototype.buffer behavior",
        body: function() {
            var buf = new ArrayBuffer(10);
            var view = new DataView(buf);

            assert.isTrue(buf === view.buffer, "buffer returns the same ArrayBuffer object passed to DataView construtor");

            // Assignment has no effect since view.buffer does not have a setter
            view.buffer = undefined;
            assert.isTrue(buf === view.buffer, "Assigning to buffer has no effect");

            descriptor = Object.getOwnPropertyDescriptor(DataView.prototype, 'buffer');

            assert.throws(function () { descriptor.get(); }, TypeError, "DataView.prototype.buffer called with no 'this' argument", "'this' is not a DataView object");
            assert.isTrue(buf === descriptor.get.call(view), "Calling getter directly returns expected buffer");

            assert.throws(function () { descriptor.get.apply(DataView.prototype) }, TypeError, "Applying getter with DataView.prototype fails", "'this' is not a DataView object");
            assert.throws(function () { DataView.prototype.buffer; }, TypeError, "Calling DataView.prototype.buffer fails", "'this' is not a DataView object");

            Object.defineProperty(DataView.prototype, "buffer", {value: 'something'});

            assert.areEqual('string', typeof DataView.prototype.buffer, "Override DataView.prototype.buffer via Object.defineProperty works");
            assert.areEqual('something', DataView.prototype.buffer, "Override DataView.prototype.buffer via Object.defineProperty works");
            assert.areEqual('something', view.buffer, "Override DataView.prototype.buffer via Object.defineProperty affects instance objects");
        }
    },
    {
        name: "DataView.prototype.byteOffset API shape",
        body: function() {
            assert.isTrue(DataView.prototype.hasOwnProperty('byteOffset'), "ArrayBuffer.prototype.hasOwnProperty('byteOffset') == true");

            descriptor = Object.getOwnPropertyDescriptor(DataView.prototype, 'byteOffset');

            assert.isTrue(descriptor !== undefined, "Object.getOwnPropertyDescriptor(DataView.prototype, 'byteOffset') !== undefined");

            assert.isTrue(descriptor.get !== undefined, "DataView.prototype.byteOffset.get !== undefined");
            assert.isTrue(typeof descriptor.get === 'function', "typeof DataView.prototype.byteOffset.get === 'function'");
            assert.areEqual(0, descriptor.get.length, "DataView.prototype.byteOffset.get.length === 0");

            assert.isTrue(descriptor.set === undefined, "DataView.prototype.byteOffset.set === undefined");

            assert.isFalse(descriptor.enumerable, "DataView.prototype.byteOffset.enumerable === false");
            assert.isTrue(descriptor.configurable, "DataView.prototype.byteOffset.configurable === true");
        }
    },
    {
        name: "DataView.prototype.byteOffset behavior",
        body: function() {
            var buf = new ArrayBuffer(10);
            var view = new DataView(buf);

            assert.areEqual(0, view.byteOffset, "byteOffset returns the same value passed to DataView construtor");

            // Assignment has no effect since view.byteOffset does not have a setter
            view.byteOffset = -1;
            assert.areEqual(0, view.byteOffset, "Assigning to byteOffset has no effect");

            descriptor = Object.getOwnPropertyDescriptor(DataView.prototype, 'byteOffset');

            assert.throws(function () { descriptor.get(); }, TypeError, "DataView.prototype.byteOffset called with no 'this' argument", "'this' is not a DataView object");
            assert.areEqual(0, descriptor.get.call(view), "Calling getter directly returns expected byteOffset");

            assert.throws(function () { descriptor.get.apply(DataView.prototype) }, TypeError, "Applying getter with DataView.prototype fails", "'this' is not a DataView object");
            assert.throws(function () { DataView.prototype.byteOffset; }, TypeError, "Calling DataView.prototype.byteOffset fails", "'this' is not a DataView object");

            Object.defineProperty(DataView.prototype, "byteOffset", {value: 'something'});

            assert.areEqual('string', typeof DataView.prototype.byteOffset, "Override DataView.prototype.byteOffset via Object.defineProperty works");
            assert.areEqual('something', DataView.prototype.byteOffset, "Override DataView.prototype.byteOffset via Object.defineProperty works");
            assert.areEqual('something', view.byteOffset, "Override DataView.prototype.byteOffset via Object.defineProperty affects instance objects");
        }
    },
    {
        name: "DataView.prototype.byteLength API shape",
        body: function() {
            assert.isTrue(DataView.prototype.hasOwnProperty('byteLength'), "ArrayBuffer.prototype.hasOwnProperty('byteLength') == true");

            descriptor = Object.getOwnPropertyDescriptor(DataView.prototype, 'byteLength');

            assert.isTrue(descriptor !== undefined, "Object.getOwnPropertyDescriptor(DataView.prototype, 'byteLength') !== undefined");

            assert.isTrue(descriptor.get !== undefined, "DataView.prototype.byteLength.get !== undefined");
            assert.isTrue(typeof descriptor.get === 'function', "typeof DataView.prototype.byteLength.get === 'function'");
            assert.areEqual(0, descriptor.get.length, "DataView.prototype.byteLength.get.length === 0");

            assert.isTrue(descriptor.set === undefined, "DataView.prototype.byteLength.set === undefined");

            assert.isFalse(descriptor.enumerable, "DataView.prototype.byteLength.enumerable === false");
            assert.isTrue(descriptor.configurable, "DataView.prototype.byteLength.configurable === true");
        }
    },
    {
        name: "DataView.prototype.byteLength behavior",
        body: function() {
            var buf = new ArrayBuffer(10);
            var view = new DataView(buf);

            assert.areEqual(10, view.byteLength, "byteLength returns the same value passed to DataView constructor");

            // Assignment has no effect since view.byteLength does not have a setter
            view.byteLength = -1;
            assert.areEqual(10, view.byteLength, "Assigning to byteLength has no effect");

            descriptor = Object.getOwnPropertyDescriptor(DataView.prototype, 'byteLength');

            assert.throws(function () { descriptor.get(); }, TypeError, "DataView.prototype.byteLength called with no 'this' argument", "'this' is not a DataView object");
            assert.areEqual(10, descriptor.get.call(view), "Calling getter directly returns expected byteLength");

            assert.throws(function () { descriptor.get.apply(DataView.prototype) }, TypeError, "Applying getter with DataView.prototype fails", "'this' is not a DataView object");
            assert.throws(function () { DataView.prototype.byteLength; }, TypeError, "Calling DataView.prototype.byteLength fails", "'this' is not a DataView object");

            Object.defineProperty(DataView.prototype, "byteLength", {value: 'something'});

            assert.areEqual('string', typeof DataView.prototype.byteLength, "Override DataView.prototype.byteLength via Object.defineProperty works");
            assert.areEqual('something', DataView.prototype.byteLength, "Override DataView.prototype.byteLength via Object.defineProperty works");
            assert.areEqual('something', view.byteLength, "Override DataView.prototype.byteLength via Object.defineProperty affects instance objects");
        }
    },
    {
        name: "BLUE: 519650, 519651, 519652 - ArrayBuffer.prototype.slice behavior with undefined or infinite arguments",
        body: function() {
            var len = 5;
            var buf = new ArrayBuffer(len);
            var u8 = new Uint8ClampedArray(buf);

            for (var i = 0; i < u8.length; i++)
            {
                u8[i] = i + 1;
            }

            var sliced = buf.slice(3, undefined);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([4,5], slicedArray, "slice(3, undefined) returns elements 3 through len-1 = [4,5]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(2, sliced.byteLength, "slice(3, undefined) returns an ArrayBuffer with byteLength = 2");

            var sliced = buf.slice(Number.POSITIVE_INFINITY, 3);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([], slicedArray, "buf.slice(Number.POSITIVE_INFINITY, 3) returns elements an empty ArrayBuffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(0, sliced.byteLength, "buf.slice(Number.POSITIVE_INFINITY, 3) returns an ArrayBuffer with byteLength = 0");

            var sliced = buf.slice(2, Number.POSITIVE_INFINITY);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([3,4,5], slicedArray, "buf.slice(2, Number.POSITIVE_INFINITY) returns elements 2 through len-1 = [3,4,5]");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(3, sliced.byteLength, "buf.slice(2, Number.POSITIVE_INFINITY) returns an ArrayBuffer with byteLength = 3");

            var sliced = buf.slice(NaN);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([1,2,3,4,5], slicedArray, "buf.slice(NaN) returns the same elements as the original ArrayBuffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(5, sliced.byteLength, "buf.slice(NaN) returns an ArrayBuffer with byteLength = 5");

            var sliced = buf.slice(Number.NEGATIVE_INFINITY);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([1,2,3,4,5], slicedArray, "buf.slice(Number.NEGATIVE_INFINITY) returns the same elements as the original ArrayBuffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(5, sliced.byteLength, "buf.slice(Number.NEGATIVE_INFINITY) returns an ArrayBuffer with byteLength = 5");

            var sliced = buf.slice(len-1,len);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([5], slicedArray, "buf.slice(len-1,len) returns the last element from the original ArrayBuffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(1, sliced.byteLength, "buf.slice(len-1,len) returns an ArrayBuffer with byteLength = 1");

            var sliced = buf.slice(len,len);
            var slicedArray = new Uint8Array(sliced);
            assert.areEqual([], slicedArray, "buf.slice(len,len) returns an empty ArrayBuffer");
            assert.isFalse(buf === sliced, "slice returns a new ArrayBuffer - not the original");
            assert.areEqual(0, sliced.byteLength, "buf.slice(len,len) returns an ArrayBuffer with byteLength = 0");
        }
    },
    {
        name: "ArrayBuffer.prototype.byteLength API shape",
        body: function() {
            assert.isTrue(ArrayBuffer.prototype.hasOwnProperty('byteLength'), "ArrayBuffer.prototype.hasOwnProperty('byteLength') == true");

            var descriptor = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength');

            assert.isTrue(descriptor !== undefined, "Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength') !== undefined");

            assert.isTrue(descriptor.get !== undefined, "ArrayBuffer.prototype.byteLength.get !== undefined");
            assert.isTrue(typeof descriptor.get === 'function', "typeof ArrayBuffer.prototype.byteLength.get === 'function'");
            assert.areEqual(0, descriptor.get.length, "ArrayBuffer.prototype.byteLength.get.length === 0");

            assert.isTrue(descriptor.set === undefined, "ArrayBuffer.prototype.byteLength.set === undefined");

            assert.isFalse(descriptor.enumerable, "ArrayBuffer.prototype.byteLength.enumerable === false");
            assert.isTrue(descriptor.configurable, "ArrayBuffer.prototype.byteLength.configurable === true");
        }
    },
    {
        name: "ArrayBuffer.prototype.byteLength behavior",
        body: function() {
            var buf = new ArrayBuffer(10);

            assert.areEqual(10, buf.byteLength, "byteLength returns the same value passed to ArrayBuffer construtor");

            // Assignment has no effect since buf.byteLength does not have a setter
            buf.byteLength = -1;
            assert.areEqual(10, buf.byteLength, "Assigning to byteLength has no effect");

            var descriptor = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength');

            assert.throws(function () { descriptor.get(); }, TypeError, "ArrayBuffer.prototype.byteLength called with no 'this' argument", "ArrayBuffer object expected");
            assert.areEqual(10, descriptor.get.call(buf), "Calling getter directly returns expected byteLength");

            assert.throws(function () { descriptor.get.apply(ArrayBuffer.prototype) }, TypeError, "Applying getter with ArrayBuffer.prototype fails", "ArrayBuffer object expected");
            assert.throws(function () { ArrayBuffer.prototype.byteLength; }, TypeError, "Calling ArrayBuffer.prototype.byteLength fails", "ArrayBuffer object expected");

            Object.defineProperty(ArrayBuffer.prototype, "byteLength", {value: 'something'});

            assert.areEqual('string', typeof ArrayBuffer.prototype.byteLength, "Override ArrayBuffer.prototype.byteLength via Object.defineProperty works");
            assert.areEqual('something', ArrayBuffer.prototype.byteLength, "Override ArrayBuffer.prototype.byteLength via Object.defineProperty works");
            assert.areEqual('something', buf.byteLength, "Override ArrayBuffer.prototype.byteLength via Object.defineProperty affects instance objects");

            Object.defineProperty(ArrayBuffer.prototype, "byteLength", {get: descriptor.get});
        }
    },
    {
        name: "BLUE: 614563 - %TypedArray%.prototype.subarray should use 0 as the default value for the begin argument",
        body: function() {
            var arr = new Uint8Array(10);

            for (var i = 0; i < arr.length; i++) {
                arr[i] = i;
            }

            assert.areEqual([0,1,2,3,4,5,6,7,8,9], arr.subarray(), "Subarray with no begin or end offsets uses 0 and length as respective default values");
        }
    },
    {
        name: "%TypedArray%.prototype.subarray uses the constructor property of the this parameter to construct the return object",
        body: function() {
            var arr = new Uint8Array(10);
            arr.constructor = Array;

            var o = arr.subarray(5, 7);

            assert.isTrue(Array.isArray(o), "%TypedArray%.prototype.subarray constructs return object using constructor property - returns an array instance from Array");
            assert.areEqual(arr.buffer, o[0], "%TypedArray%.prototype.subarray calls constructor with the first argument set to the ArrayBuffer of the this parameter");
            assert.areEqual(5, o[1], "%TypedArray%.prototype.subarray calls constructor with the second argument set to the byte offset of the begin argument");
            assert.areEqual(2, o[2], "%TypedArray%.prototype.subarray calls constructor with the third argument set to the length of the subarray");
        }
    },
    {
        name: "%TypedArray%.prototype.subarray tests on constructor access through [@@species] - special cases",
        body: function() {
            var arr = new Uint8Array(10);

            arr.constructor = undefined;
            assert.doesNotThrow(function () { arr.subarray(); }, "With [@@species] defined, calling %TypedArray%.prototype.subarray does not throw TypeError even when constructor property is undefined");

            arr.constructor = null;
            assert.throws(function () { arr.subarray(); }, TypeError, "Calling %TypedArray%.prototype.subarray throws TypeError when constructor property is null or not an object", "'[constructor]' is null or not an object");

            arr.constructor = 'some string';
            assert.throws(function () { arr.subarray(); }, TypeError, "Calling %TypedArray%.prototype.subarray throws TypeError when constructor property is null or not an object", "'[constructor]' is null or not an object");

            arr.constructor = Math.sin;
            assert.doesNotThrow(function () { arr.subarray(); }, "Calling %TypedArray%.prototype.subarray uses default typed array constructor when constructor property is not a constructor");
        }
    },
    {
        name: "ArrayBuffer.prototype.slice tests on constructor access through [@@species]",
        body: function() {
            var arr = new ArrayBuffer(10);
            arr.constructor = Array;

            assert.throws(function () { arr.slice(); }, TypeError, "Calling ArrayBuffer.prototype.slice throws TypeError when constructor function returns non-ArrayBuffer object", "ArrayBuffer object expected");

            arr.constructor = function(newLen) { return arr; }

            assert.areNotEqual(arr, arr.slice(), "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has no [@@species] property");

            arr.constructor = function(newLen) { return new ArrayBuffer(5); }

            assert.doesNotThrow(function () { arr.slice(); }, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has no [@@species] property");

            arr.constructor = function(newLen) { return new ArrayBuffer(newLen); }
            var o = arr.slice();

            assert.areEqual(10, o.byteLength, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has no [@@species] property");

            arr.constructor = function(newLen) { return new ArrayBuffer(20); }
            var o = arr.slice();

            assert.areEqual(10, o.byteLength, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has no [@@species] property");
        }
    },
    {
        name: "ArrayBuffer.prototype.slice tests on constructor access through [@@species] - special cases",
        body: function() {
            var arr = new ArrayBuffer(10);

            arr.constructor = undefined;
            assert.doesNotThrow(function () { arr.slice(); }, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has no [@@species] property");
            assert.areEqual(10, arr.slice().byteLength, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has no [@@species] property");

            arr.constructor = null;
            assert.throws(function () { arr.slice(); }, TypeError, "Calling ArrayBuffer.prototype.slice throws TypeError when constructor property is null or not an object", "'[constructor]' is null or not an object");

            arr.constructor = 'some string';
            assert.throws(function () { arr.slice(); }, TypeError, "Calling ArrayBuffer.prototype.slice throws TypeError when constructor property is null or not an object", "'[constructor]' is null or not an object");

            arr.constructor = Math.sin;
            assert.doesNotThrow(function () { arr.slice(); }, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has no [@@species] property");
            assert.areEqual(10, arr.slice().byteLength, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has no [@@species] property");
        }
    },
    {
        name: "Test of speciesConstructor codepaths accessing [@@species] through ArrayBuffer.prototype.slice",
        body: function() {
            var arr = new ArrayBuffer(10);

            arr.constructor = function() {};
            arr.constructor[Symbol.species] = undefined;
            assert.doesNotThrow(function () { arr.slice(); }, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has [@@species] == undefined");
            assert.areEqual(10, arr.slice().byteLength, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has [@@species] == undefined");

            arr.constructor[Symbol.species] = null;
            assert.doesNotThrow(function () { arr.slice(); }, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has [@@species] == undefined");
            assert.areEqual(10, arr.slice().byteLength, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has [@@species] == undefined");

            arr.constructor[Symbol.species] = {};
            assert.throws(function () { arr.slice(); }, TypeError, "Calling ArrayBuffer.prototype.slice will use default constructor if [constructor] has [@@species] == undefined", "Function 'constructor[Symbol.species]' is not a constructor");
        }
    },
    {
        name: "TypedArray constructors cannot be called without new keyword",
        body: function() {
            assert.throws(function() { Int8Array(64); }, TypeError, "Calling Int8Array() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");
            assert.throws(function() { Uint8Array(64); }, TypeError, "Calling Uint8Array() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");
            assert.throws(function() { Uint8ClampedArray(64); }, TypeError, "Calling Uint8ClampedArray() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");
            assert.throws(function() { Int16Array(64); }, TypeError, "Calling Int16Array() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");
            assert.throws(function() { Uint16Array(64); }, TypeError, "Calling Uint16Array() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");
            assert.throws(function() { Int32Array(64); }, TypeError, "Calling Int32Array() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");
            assert.throws(function() { Uint32Array(64); }, TypeError, "Calling Uint32Array() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");
            assert.throws(function() { Float32Array(64); }, TypeError, "Calling Float32Array() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");
            assert.throws(function() { Float64Array(64); }, TypeError, "Calling Float64Array() without new keyword throws TypeError", "[TypedArray]: cannot be called without the new keyword");

            assert.throws(function() { ArrayBuffer(64); }, TypeError, "Calling ArrayBuffer() without new keyword throws TypeError", "ArrayBuffer: cannot be called without the new keyword");
            assert.throws(function() { DataView(64); }, TypeError, "Calling DataView() without new keyword throws TypeError", "DataView: cannot be called without the new keyword");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
