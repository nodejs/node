// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Check that load elimination on const-marked fields works
(function() {
    function maybe_sideeffect(b) { return 42; }

    %NeverOptimizeFunction(maybe_sideeffect);

    class B {
        constructor(x) {
            this.value = x;
        }
    }
    %EnsureFeedbackVectorForFunction(B);


    function lit_const_smi() {
        let b = { value: 123 };
        maybe_sideeffect(b);
        let v1 = b.value;
        maybe_sideeffect(b);
        let v2 = b.value;
        %TurbofanStaticAssert(Object.is(v1, v2));
        %TurbofanStaticAssert(Object.is(v2, 123));
    }

    %PrepareFunctionForOptimization(lit_const_smi);
    lit_const_smi(); lit_const_smi();
    %OptimizeFunctionOnNextCall(lit_const_smi); lit_const_smi();


    function lit_const_object() {
        let o = {x: 123};
        let b = { value: o };
        maybe_sideeffect(b);
        let v1 = b.value;
        maybe_sideeffect(b);
        let v2 = b.value;
        %TurbofanStaticAssert(Object.is(v1, v2));
        %TurbofanStaticAssert(Object.is(v2, o));
    }

    %PrepareFunctionForOptimization(lit_const_object);
    lit_const_object(); lit_const_object();
    %OptimizeFunctionOnNextCall(lit_const_object); lit_const_object();


    function lit_computed_smi(k) {
        let kk = 2 * k;
        let b = { value: kk };
        maybe_sideeffect(b);
        let v1 = b.value;
        maybe_sideeffect(b);
        let v2 = b.value;
        %TurbofanStaticAssert(Object.is(v1, v2));
        %TurbofanStaticAssert(Object.is(v2, kk));
    }

    %PrepareFunctionForOptimization(lit_computed_smi);
    lit_computed_smi(1); lit_computed_smi(2);
    %OptimizeFunctionOnNextCall(lit_computed_smi); lit_computed_smi(3);

    // TODO(bmeurer): Fix const tracking for double fields in object literals
    // lit_computed_smi(1.1); lit_computed_smi(2.2);
    // %OptimizeFunctionOnNextCall(lit_computed_smi); lit_computed_smi(3.3);


    function lit_param_object(k) {
        let b = { value: k };
        maybe_sideeffect(b);
        let v1 = b.value;
        maybe_sideeffect(b);
        let v2 = b.value;
        %TurbofanStaticAssert(Object.is(v1, v2));
        %TurbofanStaticAssert(Object.is(v2, k));
    }

    %PrepareFunctionForOptimization(lit_param_object);
    lit_param_object({x: 1}); lit_param_object({x: 2});
    %OptimizeFunctionOnNextCall(lit_param_object); lit_param_object({x: 3});


    function nested_lit_param(k) {
        let b = { x: { value: k } };
        maybe_sideeffect(b);
        let v1 = b.x.value;
        maybe_sideeffect(b);
        let v2 = b.x.value;
        %TurbofanStaticAssert(Object.is(v1, v2));
        %TurbofanStaticAssert(Object.is(v2, k));
    }

    %PrepareFunctionForOptimization(nested_lit_param);
    nested_lit_param(1); nested_lit_param(2);
    %OptimizeFunctionOnNextCall(nested_lit_param); nested_lit_param(3);

    // TODO(bmeurer): Fix const tracking for double fields in object literals
    // nested_lit_param(1.1); nested_lit_param(2.2);
    // %OptimizeFunctionOnNextCall(nested_lit_param); nested_lit_param(3.3);


    function nested_lit_param_object(k) {
        let b = { x: { value: k } };
        maybe_sideeffect(b);
        let v1 = b.x.value;
        maybe_sideeffect(b);
        let v2 = b.x.value;
        %TurbofanStaticAssert(Object.is(v1, v2));
        %TurbofanStaticAssert(Object.is(v2, k));
    }

    %PrepareFunctionForOptimization(nested_lit_param_object);
    nested_lit_param_object({x: 1}); nested_lit_param_object({x: 2});
    %OptimizeFunctionOnNextCall(nested_lit_param_object);
    nested_lit_param_object({x: 3});


    function inst_param(k) {
        let b = new B(k);
        maybe_sideeffect(b);
        let v1 = b.value;
        maybe_sideeffect(b);
        let v2 = b.value;
        if (!%IsDictPropertyConstTrackingEnabled()) {
          // TODO(v8:11457) If v8_dict_property_const_tracking is enabled, then
          // b has a dictionary mode prototype and the load elimination doesn't
          // work, yet.
          %TurbofanStaticAssert(Object.is(v1, v2));
          %TurbofanStaticAssert(Object.is(v2, k));
        }
    }

    %EnsureFeedbackVectorForFunction(B);
    %PrepareFunctionForOptimization(inst_param);
    inst_param(1); inst_param(2);
    %OptimizeFunctionOnNextCall(inst_param); inst_param(3);

    // TODO(gsps): Reenable once we fully support const field information
    //   tracking in the presence of pointer compression.
    // inst_param(1.1); inst_param(2.2);
    // %OptimizeFunctionOnNextCall(inst_param); inst_param(3.3);

    %PrepareFunctionForOptimization(inst_param);
    inst_param({x: 1}); inst_param({x: 2});
    %OptimizeFunctionOnNextCall(inst_param); inst_param({x: 3});


    function inst_computed(k) {
        let kk = 2 * k;
        let b = new B(kk);
        maybe_sideeffect(b);
        let v1 = b.value;
        maybe_sideeffect(b);
        let v2 = b.value;
        if (!%IsDictPropertyConstTrackingEnabled()) {
          // TODO(v8:11457) If v8_dict_property_const_tracking is enabled, then
          // b has a dictionary mode prototype and the load elimination doesn't
          // work, yet.
          %TurbofanStaticAssert(Object.is(v1, v2));
          %TurbofanStaticAssert(Object.is(v2, kk));
        }
    }

    %EnsureFeedbackVectorForFunction(B);
    %PrepareFunctionForOptimization(inst_computed);
    inst_computed(1); inst_computed(2);
    %OptimizeFunctionOnNextCall(inst_computed); inst_computed(3);

    %PrepareFunctionForOptimization(inst_computed);
    inst_computed(1.1); inst_computed(2.2);
    %OptimizeFunctionOnNextCall(inst_computed); inst_computed(3.3);
})();
