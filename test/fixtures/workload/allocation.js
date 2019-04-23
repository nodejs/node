'use strict';

const util = require('util');
const total = parseInt(process.env.TEST_ALLOCATION) || 100;
let count = 0;
let string = '';
function runAllocation() {
  string += util.inspect(process.env);
  if (count++ < total) {
    setTimeout(runAllocation, 1);
  } else {
    console.log(string.length);
  }
}

setTimeout(runAllocation, 1);
