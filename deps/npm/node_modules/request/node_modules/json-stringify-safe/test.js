var stringify = require('./stringify.js');

var circularObj = { a: 'b' };
circularObj.circularRef = circularObj;
circularObj.list = [ circularObj, circularObj ];

//////////
// default
var testObj = {
  "a": "b",
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


////////
// prune
testObj = {
  "a": "b",
  "list": [
    null,
    null
  ]
};

function prune(k, v) {}

assert.equal(JSON.stringify(testObj, null, 2),
             stringify(circularObj, null, 2, prune));

///////////
// re-cycle
// (throws)
function recycle(k, v) {
  return v;
}

assert.throws(function() {
  stringify(circularObj, null, 2, recycle);
});

////////
// fancy
testObj = {
  "a": "b",
  "circularRef": "circularRef{a:string,circularRef:Object,list:Array}",
  "list": [
    "0{a:string,circularRef:Object,list:Array}",
    "1{a:string,circularRef:Object,list:Array}"
  ]
};

function signer(key, value) {
  var ret = key + '{';
  var f = false;
  for (var i in value) {
    if (f)
      ret += ',';
    f = true;
    ret += i + ':';
    var v = value[i];
    switch (typeof v) {
      case 'object':
        if (!v)
          ret += 'null';
        else if (Array.isArray(v))
          ret += 'Array'
        else
          ret += v.constructor && v.constructor.name || 'Object';
        break;
      default:
        ret += typeof v;
        break;
    }
  }
  ret += '}';
  return ret;
}

assert.equal(JSON.stringify(testObj, null, 2),
             stringify(circularObj, null, 2, signer));


////////
// pass!
console.log('ok');
