'use strict';
function fib(n) {
  if (n === 0 || n === 1) return n;
  return fib(n - 1) + fib(n - 2);
}
fib(parseInt(process.argv[2]) || 35);
process.exit(55);
