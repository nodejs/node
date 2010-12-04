var common = require('../common');
var assert = require('assert');

console.log('process.pid: ' + process.pid);

var first = 0,
    second = 0;

process.addListener('SIGUSR1', function() {
  console.log('Interrupted by SIGUSR1');
  first += 1;
});

process.addListener('SIGUSR1', function() {
  second += 1;
  setTimeout(function() {
    console.log('End.');
    process.exit(0);
  }, 5);
});

var i = 0;
setInterval(function() {
  console.log('running process...' + ++i);

  if (i == 5) {
    process.kill(process.pid, 'SIGUSR1');
  }
}, 1);


process.addListener('exit', function() {
  assert.equal(1, first);
  assert.equal(1, second);
});
