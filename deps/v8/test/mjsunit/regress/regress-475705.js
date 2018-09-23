// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Crankshaft changes the stack usage and messes up the binary search for the
// stack depth that causes a stack overflow.  The issue only arises without
// regexp optimization, which can happen on pages that create a lot of regexps.
// Flags: --noopt --noregexp-optimization

// Should not crash with a stack overflow in the regexp compiler, even when the
// JS has used most of the stack.
function use_space_then_do_test(depth) {
  try {
    // The "+ depth" is to avoid the regexp compilation cache.
    var regexp_src = repeat(".(.)", 12) + depth;
    use_space(depth, regexp_src);
    return true;
  } catch (e) {
    assertFalse(("" + e).indexOf("tack") == -1);  // Check for [Ss]tack.
    return false;
  }
}

function use_space(n, regexp_src) {
  if (--n == 0) {
    do_test(regexp_src);
    return;
  }
  use_space(n, regexp_src);
}

function repeat(str, n) {
  var answer = "";
  while (n-- != 0) {
    answer += str;
  }
  return answer;
}

var subject = repeat("y", 200);

function do_test(regexp_src) {
  var re = new RegExp(regexp_src);
  re.test(subject);
}

function try_different_stack_limits() {
  var lower = 100;
  var higher = 100000;
  while (lower < higher - 1) {
    var average = Math.floor((lower + higher) / 2);
    if (use_space_then_do_test(average)) {
      lower = average;
    } else {
      higher = average;
    }
  }
  for (var i = lower - 5; i < higher + 5; i++) {
    use_space_then_do_test(i);
  }
}

try_different_stack_limits();
