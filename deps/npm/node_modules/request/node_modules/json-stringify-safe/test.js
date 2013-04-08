var stringify = require('./stringify.js');

var circularObj = {};
circularObj.circularRef = circularObj;
circularObj.list = [ circularObj, circularObj ];

var testObj = {
  "circularRef": "[Circular]",
  "list": [
    "[Circular]",
    "[Circular]"
  ]
};

var assert = require('assert');
assert.equal(JSON.stringify(testObj, null, 2),
             stringify(circularObj, null, 2));

assert.equal(JSON.stringify(testObj, null, 2),
            JSON.stringify(circularObj, stringify.getSerialize(), 2));

console.log('ok');
