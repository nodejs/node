// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function* gen_yield(o) {
  eval("");
  do {
    with (o) {
      return;
    }
  } while (yield);
  yield;
  return x;
}

function* gen_yield_star(o) {
  eval("");
  do {
    with (o) {
      return;
    }
  } while (yield* o);
  return x;
}

function* gen_try_finally(o) {
  eval("");
  l: {
    try {
      if (o) return 1;
      if (o) break l;
    } finally {
      with (o) {
        return 2;
      }
    }
  }
  yield;
  return x;
}

function* gen_switch(o) {
  eval("");
  if (true || o) {
    with (o) {
      return;
    }
  } else switch (o) {
    case 0:
      yield;
  }
  return x;
}

function* gen_switch_table(o) {
  eval("");
  if (true || o) {
    with (o) {
      return;
    }
  } else switch (o) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      yield;
  }
  return x;
}

function* gen_switch_throwing_tag(o) {
  eval("");
  try {
    with (o) {
      switch (o() = 1) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
          yield;
      }
    }
  } catch {}
  yield;
  return 42;
}

for (let i = 0; i < 1000; i++) {
  gen_yield({}).next();
  gen_yield_star({}).next();
  gen_try_finally({}).next();
  gen_switch({}).next();
  gen_switch_table({}).next();
  gen_switch_throwing_tag(() => {}).next();
}
