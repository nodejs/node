require('../common');
var util = require('util');
var assert = require('assert');
var exec = require('child_process').exec;

var success_count = 0;
var error_count = 0;

var cmd = ['"' + process.execPath + '"', '-e', '"console.error(process.argv)"',
           'foo', 'bar'].join(' ');
var expected = util.format([process.execPath, 'foo', 'bar']) + '\n';
var child = exec(cmd, function(err, stdout, stderr) {
  if (err) {
    console.log(err.toString());
    ++error_count;
    return;
  }
  assert.equal(stderr, expected);
  ++success_count;
});

process.on('exit', function() {
  assert.equal(1, success_count);
  assert.equal(0, error_count);
});

