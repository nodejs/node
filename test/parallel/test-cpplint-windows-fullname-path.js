'use strict';
require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

var child;
const EXPECTATION = 'tools/cpplint.py';


try {
  process.chdir('./tools');
} catch (err) {
  throw err;
}

child = spawn(
  'python', [
    '-c',
    'import cpplint as c; print(c.FileInfo("cpplint.py").RepositoryName())'
  ]
);

var response = '';

child.stdout.setEncoding('utf8');
child.stdout.on('data', function(chunk) {
  response += chunk;
});

child.on('close', function() {
  assert.equal(response.replace(/\n/,'').replace(/\r/, ''), EXPECTATION);
  process.chdir('../');
});
