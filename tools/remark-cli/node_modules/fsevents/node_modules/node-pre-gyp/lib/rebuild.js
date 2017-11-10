"use strict";

module.exports = exports = rebuild;

exports.usage = 'Runs "clean" and "build" at once';

function rebuild (gyp, argv, callback) {
  gyp.todo.unshift(
      { name: 'clean', args: [] },
      { name: 'build', args: ['rebuild'] }
  );
  process.nextTick(callback);
}
