// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Specific regression test for crbug.com/1236694.
let long = '1000000000000000000000000000000000000000000000'.repeat(20) + '0';
let short = '100000000000000000000000000000000000000000000'.repeat(20) + '0';
BigInt(long).toLocaleString();
BigInt(short).toLocaleString();

// Generalized to test a range of similar inputs. Considerations to keep
// execution times reasonable while testing interesting cases:
// - The number of zeros should grow large enough to potentially fill two
//   entire digits (i.e. >= 38), which makes the recursion take the early
//   termination path, which is worthy of test coverage.
// - The number of repeats should grow large enough to shift any bug-triggering
//   bit pattern to any position in a digit, i.e. >= 64.
// - Fewer repeats may be easier to debug in case of failure, but likely don't
//   provide additional test coverage, so we test very few distinct values.
// - To test the fast algorithm, (zeros+1)*repeats must be >= 810 or so.
function test(zeros, repeats) {
  let chunk = '1' + '0'.repeat(zeros);
  let input = chunk.repeat(repeats);
  assertEquals(input, BigInt(input).toString(),
               `bug for ${zeros} zeros repeated ${repeats} times`);
}
for (let zeros = 1; zeros < 50; zeros++) {
  for (let repeats = 64; repeats > 0; repeats -= 20) {
    test(zeros, repeats);
  }
}
test(96, 11);  // Found to hit the extra-early recursion termination path.
