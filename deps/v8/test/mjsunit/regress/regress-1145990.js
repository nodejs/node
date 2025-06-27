// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class B { }

class C extends B {
  #field = 'test';
}

for (let i = 0; i < 10000; i++) {
  new C();
}
