'use strict';

const { spawnSync } = require("child_process");
const sleepTime = new Number(process.argv[2] || "0.1");
const repeat = new Number(process.argv[3]) || 5;

function functionOne() {
  functionTwo();
}

function functionTwo() {
  spawnSync('sleep', [`${sleepTime}`]);
}

for (let i = 0; i < repeat; i++) {
  functionOne();
}
