// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --maglev --turbofan
// Flags: --private-field-bytecodes


class TestClass {
    #privateField = 0;
    publicField = 0;

    constructor() {
        this.#privateField = 10;
        this.publicField = 20;
    }

    // Should generate: SetPrivateField
    setPrivate(val) {
        this.#privateField = val;
    }

    // Should generate: SetPrivateField
    incPrivate(){
        this.#privateField++;
    }

    add2Private(x){
        this.#privateField+=x;
    }

    // Should generate: GetPrivateField
    getPrivate() {
        return this.#privateField;
    }

    // Should generate: SetNamedProperty
    setPublic(val) {
        this.publicField = val;
    }

    // Should generate: LoadNamedProperty
    getPublic() {
        return this.publicField;
    }

    force_context_depth() {
        // 1. Force a physical Context object on the heap that cannot be optimized away.
        // We fill it with dummy variables so the context array has occupied slots.
        let pad1 = "garbage_1";
        let pad2 = "garbage_2";
        let pad3 = "garbage_3";
        let pad4 = "garbage_4";

        eval(""); // Defeats Context Allocation Elimination!
        this.#privateField += 5;

        return this.#privateField;
    }
}

function test_private_fields() {
    const instance = new TestClass();
    let val = instance.force_context_depth();

    instance.setPrivate(42);
    instance.getPrivate();
    instance.setPublic(100);
    instance.getPublic();

    instance.incPrivate();
    instance.add2Private(2);
    return [instance.getPrivate(),instance.getPublic(), val]
}

function assert_test_private_fields(arr){
    assertEquals(arr[0], 45);
    assertEquals(arr[1], 100);
    assertEquals(arr[2], 15);
}

// prettier-ignore
function run(){
test_private_fields();

const instance = new TestClass();

%PrepareFunctionForOptimization(TestClass.prototype.setPrivate);
%PrepareFunctionForOptimization(TestClass.prototype.getPrivate);
%PrepareFunctionForOptimization(TestClass.prototype.incPrivate);
%PrepareFunctionForOptimization(TestClass.prototype.add2Private);
%PrepareFunctionForOptimization(TestClass.prototype.force_context_depth);
%PrepareFunctionForOptimization(test_private_fields);

assert_test_private_fields(test_private_fields());

%CompileBaseline(TestClass.prototype.setPrivate);
%CompileBaseline(TestClass.prototype.getPrivate);
%CompileBaseline(TestClass.prototype.incPrivate);
%CompileBaseline(TestClass.prototype.add2Private);
%CompileBaseline(TestClass.prototype.force_context_depth);
%CompileBaseline(test_private_fields);

assert_test_private_fields(test_private_fields());

%OptimizeMaglevOnNextCall(TestClass.prototype.setPrivate);
%OptimizeMaglevOnNextCall(TestClass.prototype.getPrivate);
%OptimizeMaglevOnNextCall(TestClass.prototype.incPrivate);
%OptimizeMaglevOnNextCall(TestClass.prototype.add2Private);
%OptimizeMaglevOnNextCall(TestClass.prototype.force_context_depth);
%OptimizeMaglevOnNextCall(test_private_fields);

assert_test_private_fields(test_private_fields());

// assertTrue(isMaglevved(TestClass.prototype.setPrivate));
// assertTrue(isMaglevved(TestClass.prototype.getPrivate));
// assertTrue(isMaglevved(TestClass.prototype.incPrivate));
// assertTrue(isMaglevved(TestClass.prototype.add2Private));
// assertTrue(isMaglevved(TestClass.prototype.force_context_depth));
// assertTrue(isMaglevved(test_private_fields));

assert_test_private_fields(test_private_fields());

%OptimizeFunctionOnNextCall(TestClass.prototype.setPrivate);
%OptimizeFunctionOnNextCall(TestClass.prototype.getPrivate);
%OptimizeFunctionOnNextCall(TestClass.prototype.incPrivate);
%OptimizeFunctionOnNextCall(TestClass.prototype.add2Private);
%OptimizeFunctionOnNextCall(TestClass.prototype.force_context_depth);
%OptimizeFunctionOnNextCall(test_private_fields);

assert_test_private_fields(test_private_fields());

// assertOptimized(TestClass.prototype.setPrivate);
// assertOptimized(TestClass.prototype.getPrivate);
// assertOptimized(TestClass.prototype.incPrivate);
// assertOptimized(TestClass.prototype.add2Private);
// assertOptimized(TestClass.prototype.force_context_depth);
// assertOptimized(test_private_fields);

assert_test_private_fields(test_private_fields());

}

run();
