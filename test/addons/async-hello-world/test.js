var assert = require('assert');
var binding = require('./build/Release/binding');
binding(5, function (err, val) {
  assert.equal(null, err);
  assert.equal(10, val);
  console.error('done :)');
});
