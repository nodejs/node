'use strict';
let counter = 0;
let result;
const TEST_INTERVALS = parseInt(process.env.TEST_INTERVALS) || 1;

const i = setInterval(function interval() {
  counter++;
  if (counter < TEST_INTERVALS) {
    result = 1;
  } else {
    result = process.hrtime();
    clearInterval(i);
  }
}, 100);
