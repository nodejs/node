// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

const proto = {value: 1};
const home = {
  __proto__: proto,
  read() {
    return super.value;
  },
};

// Keep the lookup-start object in dictionary mode when the IC observes it.
for (let i = 0; %HasFastProperties(proto); ++i) {
  proto[`padding${i}`] = i;
}

const receiver = {marker: 42};

%PrepareFunctionForOptimization(home.read);
assertEquals(1, home.read.call(receiver));
assertEquals(1, home.read.call(receiver));
%OptimizeFunctionOnNextCall(home.read);
assertEquals(1, home.read.call(receiver));

let observed_receiver;
Object.defineProperty(proto, 'value', {
  configurable: true,
  get() {
    observed_receiver = this;
    return this.marker;
  },
});

assertEquals(42, home.read.call(receiver));
assertSame(receiver, observed_receiver);
assertOptimized(home.read);
