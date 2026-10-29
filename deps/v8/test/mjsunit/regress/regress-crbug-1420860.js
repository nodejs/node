// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const v1 = 605.5772560725025.toLocaleString();
const v2 = [723.31143385306,-2.220446049250313e-16,-2.220446049250313e-16,-730.3736786091615,-1.7184190427817423e+308];
const v3 = v2.__proto__;
const v4 = v2.join(v1);
for (let v5 = 0; v5 < 66; v5++) {
    v2.valueOf = v2["join"](v2.join(v4));
    v2.unshift(v3);
}
