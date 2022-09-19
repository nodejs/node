// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-shadow-realm

const shadowRealm = new ShadowRealm();

// bind
var wrapped = shadowRealm.evaluate('function foo(bar, quz) {}; foo');
assertEquals(wrapped.name, 'foo');
assertEquals(wrapped.length, 2);

var bound = wrapped.bind(undefined, 'bar');
assertEquals(bound.name, 'bound foo');
assertEquals(bound.length, 1);

// proxy
var wrapped = shadowRealm.evaluate('function foo(bar, quz) {}; foo');
assertEquals(wrapped.name, 'foo');
assertEquals(wrapped.length, 2);

var proxy = new Proxy(wrapped, {});
assertEquals(proxy.name, 'foo');
assertEquals(proxy.length, 2);
