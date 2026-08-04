// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Outer {
    constructor(o) { this.x = o; }
}

class Inner {
    constructor(o) { this.y = o; }
}

class ArrayHolder {
    constructor(o) {
        this.array = [];
        this.array[1] = o;
    }
}

const root = {};
root.first = new Outer(
    new ArrayHolder(
        new Inner(root)
    )
);

JSON.stringify(root);
