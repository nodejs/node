'use strict';

process.stdout.write(`${process.pid}`);

const wantOptimization = !process.argv.includes('--no-opt');

const testRegex = /test-regex/gi;

function functionOne() {
  for (let i = 0; i < 100; i++) {
    const match = testRegex.exec(Math.random().toString());
  }
}

function functionTwo() {
  functionOne();
}

if (wantOptimization) {
  %PrepareFunctionForOptimization(functionOne);
  %PrepareFunctionForOptimization(functionTwo);
}

functionTwo();

if (wantOptimization) {
  %OptimizeFunctionOnNextCall(functionOne);
  %OptimizeFunctionOnNextCall(functionTwo);
}

functionTwo();
