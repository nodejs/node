'use strict';
function fib(n) {
  if (n === 0 || n === 1) return n;
  return fib(n - 1) + fib(n - 2);
}

// This is based on emperial values - in the CI, on Windows the program
// tend to finish too fast then we won't be able to see the profiled script
// in the samples, so we need to bump the values a bit. On slower platforms
// like the Pis it could take more time to complete, we need to use a
// smaller value so the test would not time out.
const FIB = process.platform === 'win32' ? 40 : 30;

const n = parseInt(process.env.FIB) || FIB;
process.stdout.write(`${fib(n)}\n`);
