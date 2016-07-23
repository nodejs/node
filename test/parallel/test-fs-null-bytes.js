'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var expectedError = /Path must be a string without null bytes/;

function check(async, sync) {
  var argsSync = Array.prototype.slice.call(arguments, 2);
  var argsAsync = argsSync.concat(function(er) {
    assert(er && er.message.match(expectedError));
    assert.equal(er.code, 'ENOENT');
  });

  if (sync)
    assert.throws(function() {
      console.error(`fs.${sync}()`, argsSync);
      fs[sync].apply(null, argsSync);
    }, expectedError);

  if (async) {
    console.error(`fs.${async}()`, argsAsync);
    fs[async].apply(null, argsAsync);
  }
}

check('access', 'accessSync', 'foo\u0000bar');
check('access', 'accessSync', 'foo\u0000bar', 'F_OK');
check('appendFile', 'appendFileSync', 'foo\u0000bar');
check('chmod', 'chmodSync', 'foo\u0000bar', '0644');
check('chown', 'chownSync', 'foo\u0000bar', 12, 34);
check('link', 'linkSync', 'foo\u0000bar', 'foobar');
check('link', 'linkSync', 'foobar', 'foo\u0000bar');
check('lstat', 'lstatSync', 'foo\u0000bar');
check('mkdir', 'mkdirSync', 'foo\u0000bar', '0755');
check('open', 'openSync', 'foo\u0000bar', 'r');
check('readFile', 'readFileSync', 'foo\u0000bar');
check('readdir', 'readdirSync', 'foo\u0000bar');
check('readlink', 'readlinkSync', 'foo\u0000bar');
check('realpath', 'realpathSync', 'foo\u0000bar');
check('rename', 'renameSync', 'foo\u0000bar', 'foobar');
check('rename', 'renameSync', 'foobar', 'foo\u0000bar');
check('rmdir', 'rmdirSync', 'foo\u0000bar');
check('stat', 'statSync', 'foo\u0000bar');
check('symlink', 'symlinkSync', 'foo\u0000bar', 'foobar');
check('symlink', 'symlinkSync', 'foobar', 'foo\u0000bar');
check('truncate', 'truncateSync', 'foo\u0000bar');
check('unlink', 'unlinkSync', 'foo\u0000bar');
check(null, 'unwatchFile', 'foo\u0000bar', common.fail);
check('utimes', 'utimesSync', 'foo\u0000bar', 0, 0);
check(null, 'watch', 'foo\u0000bar', common.fail);
check(null, 'watchFile', 'foo\u0000bar', common.fail);
check('writeFile', 'writeFileSync', 'foo\u0000bar');

// an 'error' for exists means that it doesn't exist.
// one of many reasons why this file is the absolute worst.
fs.exists('foo\u0000bar', function(exists) {
  assert(!exists);
});
assert(!fs.existsSync('foo\u0000bar'));

function checkRequire(arg) {
  assert.throws(function() {
    console.error(`require(${JSON.stringify(arg)})`);
    require(arg);
  }, expectedError);
}

checkRequire('\u0000');
checkRequire('foo\u0000bar');
checkRequire('foo\u0000');
checkRequire('foo/\u0000');
checkRequire('foo/\u0000.js');
checkRequire('\u0000/foo');
checkRequire('./foo/\u0000');
checkRequire('./\u0000/foo');
