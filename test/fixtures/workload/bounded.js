'use strict';

const total = parseInt(process.env.TEST_ALLOCATION) || 5000;
const chunk = parseInt(process.env.TEST_CHUNK) || 1000;
const cleanInterval = parseInt(process.env.TEST_CLEAN_INTERVAL) || 100;
let count = 0;
let arr = [];
function runAllocation() {
  count++;
  if (count < total) {
    if (count % cleanInterval === 0) {
      arr.splice(0, arr.length);
      setImmediate(runAllocation);
    } else {
      const str = JSON.stringify(process.config).slice(0, chunk);
      arr.push(str);
      setImmediate(runAllocation);
    }
  }
}

setImmediate(runAllocation);
