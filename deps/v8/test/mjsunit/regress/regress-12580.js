// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Foo extends function () {
    return new Proxy(Object.create(new.target.prototype), {}); } {
  #bar = 7;
  has() { return #bar in this; }
};

assertTrue((new Foo()).has());
