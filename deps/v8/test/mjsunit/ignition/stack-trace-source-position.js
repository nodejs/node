// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-turbo

// TODO(yangguo): fix for turbofan

function f(x) {
  if (x == 0) {
    return new Error().stack;
  }
  return f(x - 1);
}

var stack_lines = f(2).split("\n");

assertTrue(/at f \(.*?:11:12\)/.test(stack_lines[1]));
assertTrue(/at f \(.*?:13:10\)/.test(stack_lines[2]));
assertTrue(/at f \(.*?:13:10\)/.test(stack_lines[3]));
