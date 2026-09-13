// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fast-proxy-ic --allow-natives-syntax --no-maglev

// A super property load whose home object's prototype is a Proxy must invoke
// the trap on that prototype, passing `this` as the [[Get]] receiver. Turbofan's
// proxy fast path (ReduceProxyAccess) treated input 0 (the receiver) as the
// proxy, so when `this` was itself a Proxy of the same map it called the trap
// on the wrong target and returned the wrong value.

const homeProtoTarget = {foo: 'from-home-proto'};
const thisTarget = {foo: 'from-this'};
const handler = {
  get(t, k, r) {
    return Reflect.get(t, k, r);
  }
};

// Two proxies constructed identically, hence sharing a map: this is what let
// the (bogus) receiver map check on `this` pass in the miscompiled fast path.
const homeProto = new Proxy(homeProtoTarget, handler);
const thisProxy = new Proxy(thisTarget, handler);
assertTrue(%HaveSameMap(homeProto, thisProxy));

const home = {
  __proto__: homeProto,
  getFoo() {
    return super.foo;
  },
};

function run() {
  return home.getFoo.call(thisProxy);
}

%PrepareFunctionForOptimization(handler.get);
%PrepareFunctionForOptimization(home.getFoo);
%PrepareFunctionForOptimization(run);
assertEquals('from-home-proto', run());
assertEquals('from-home-proto', run());
%OptimizeFunctionOnNextCall(home.getFoo);
run();
%OptimizeFunctionOnNextCall(run);
assertEquals('from-home-proto', run());
