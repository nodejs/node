'use strict';
require('../common');
const fork = require('child_process').fork;

if (process.argv[2] === 'child') {
  console.log('child -> call disconnect');
  process.disconnect();

  setTimeout(function() {
    console.log('child -> will this keep it alive?');
    process.on('message', function() { });
  }, 400);

} else {
  const child = fork(__filename, ['child']);

  child.on('disconnect', function() {
    console.log('parent -> disconnect');
  });

  child.once('exit', function() {
    console.log('parent -> exit');
  });
}
