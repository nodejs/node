var assert = require('assert');
var binding = require('./out/Release/binding');
var isolates = process.binding('isolates');

console.log("binding.length =", binding.length);

if (process.tid === 1) {
  var isolate = isolates.create(process.argv);
  for (var i = 0; i < binding.length; i++) {
    console.log('parent',
                'binding.set(' + i + ', ' + i + ')',
                binding.set(i, i));
  }
} else {
  for (var i = 0; i < binding.length; i++) {
    console.log('child', 'binding.get(' + i + ')', binding.get(i));
  }
}

