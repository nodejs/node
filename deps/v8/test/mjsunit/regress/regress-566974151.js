// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Object elements fast path.
{
  const a = [[{}], 1];
  const depth = {
    valueOf() {
      a.push(99);
      return 1;
    }
  };
  assertEquals([{}, 1], a.flat(depth));
}

// Double elements fast path (with holey double sub-array to trigger descent).
{
  const sub = [1.5, , 2.5];
  const a = [sub, 3.5];
  const depth = {
    valueOf() {
      a.push(99.5);
      return 1;
    }
  };
  assertEquals([1.5, 2.5, 3.5], a.flat(depth));
}

// Double elements fast path (with packed Smi sub-array to trigger descent in
// the copy pass).
{
  const a = [[1], 2.5];
  const depth = {
    valueOf() {
      a.push([]);
      return 1;
    }
  };
  assertEquals([1, 2.5], a.flat(depth));
}

// Smi elements fast path (with holey Smi sub-array to trigger descent).
{
  const sub = [1, , 2];
  const a = [sub, 3];
  const depth = {
    valueOf() {
      a.push(99);
      return 1;
    }
  };
  assertEquals([1, 2, 3], a.flat(depth));
}
