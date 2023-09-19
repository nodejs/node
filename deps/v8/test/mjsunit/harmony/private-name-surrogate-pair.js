// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class C1 {
  #𖥸 = 42;
  m() { return this.#𖥸; }
}

assertEquals((new C1).m(), 42);

class C2 {
  #𖥸() { return 42; }
  m() { return this.#𖥸(); }
}

assertEquals((new C2).m(), 42);
