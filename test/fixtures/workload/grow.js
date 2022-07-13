'use strict';

const chunk = parseInt(process.env.TEST_CHUNK) || 1000;

let arr = [];
function runAllocation() {
  const str = JSON.stringify(process.config).slice(0, chunk);
  arr.push(str);
  setImmediate(runAllocation);
}

setImmediate(runAllocation);
