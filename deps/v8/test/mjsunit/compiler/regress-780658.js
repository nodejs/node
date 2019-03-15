// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function get1(l, b) {
    return l[1];
}

function with_double(x) {
    var o = {a: [x,x,x]};
    o.a.some_property = 1;
    return get1(o.a);
}

function with_tagged(x) {
    var l = [{}, x,x];
    return get1(l);
}

%PrepareFunctionForOptimization(with_double);
with_double(.5);
with_tagged({});
with_double(.6);
with_tagged(null);
with_double(.5);
with_tagged({});
%OptimizeFunctionOnNextCall(with_double);
with_double(.7);
