// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addProperties(o)
{
    o.p1 = 1;
    o.p2 = 2;
    o.p3 = 3;
    o.p4 = 4;
    o.p5 = 5;
    o.p6 = 6;
    o.p7 = 7;
    o.p8 = 8;
}
function removeProperties(o)
{
    delete o.p8;
    delete o.p7;
    delete o.p6;
    delete o.p5;
}
function makeO()
{
    var o = { };
    addProperties(o);
    removeProperties(o);
    addProperties(o);
}
for (var i = 0; i < 3; ++i) {
    o = makeO();
}
