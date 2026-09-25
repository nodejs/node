'use strict';
const v8 = require('v8');

setTimeout(() => {
  v8.takeCoverage();
}, 1000);

setTimeout(() => {
  v8.takeCoverage();
}, 2000);
