"use strict";

module.exports = exports = rebuild;

exports.usage = 'Runs "clean" and "install" at once';

function rebuild (gyp, argv, callback) {
  gyp.todo.unshift(
      { name: 'clean', args: [] },
      { name: 'install', args: [] }
  );
  process.nextTick(callback);
}
