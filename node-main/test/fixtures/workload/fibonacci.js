'use strict';
function fib(n) {
  if (n === 0 || n === 1) return n;
  return fib(n - 1) + fib(n - 2);
}

const n = parseInt(process.env.FIB, 10) || 40;
process.stdout.write(`${fib(n)}\n`);
