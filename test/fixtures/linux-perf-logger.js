'use strict';

process.stdout.write(`${process.pid}`);

const testRegex = /test-regex/gi;

function functionOne() {
  for (let i = 0; i < 100; i++) {
    const match = testRegex.exec(Math.random().toString());
  }
}

function functionTwo() {
  functionOne();
}

functionTwo();
