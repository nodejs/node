'use strict';
const test = require('node:test');

// Multiple statements on one line — only first executes if condition met
function riskyOperation(value) {
  if (value > 0) return value * 2; return -1;
}

// Single line if/else — only one branch runs per call
function classify(n) {
  if (n > 0) return 'positive'; else return 'non-positive';
}

// One-liner function where early validation can skip later statements
function processEntry(a) { if (!a) return null; return String(a).toUpperCase(); }

// Uncalled function with multiple inline statements — 0% statement coverage
function neverCalled() { const a = 1; const b = 2; return a + b; }

test('multistatement coverage', () => {
  riskyOperation(5);    // takes early return, second statement skipped
  classify(10);         // takes positive branch, else branch skipped
  processEntry('hello'); // passes guard, both statements execute
});
