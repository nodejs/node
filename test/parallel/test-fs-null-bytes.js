'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

function check(async, sync) {
  var expected = /Path must be a string without null bytes/;
  var argsSync = Array.prototype.slice.call(arguments, 2);
  var argsAsync = argsSync.concat(function(er) {
    assert(er && er.message.match(expected));
    assert.equal(er.code, 'ENOENT');
  });

  if (sync)
    assert.throws(function() {
      console.error(sync.name, argsSync);
      sync.apply(null, argsSync);
    }, expected);

  if (async)
    async.apply(null, argsAsync);
}

check(fs.access,      fs.accessSync,      'foo\u0000bar');
check(fs.access,      fs.accessSync,      'foo\u0000bar', fs.F_OK);
check(fs.appendFile,  fs.appendFileSync,  'foo\u0000bar');
check(fs.chmod,       fs.chmodSync,       'foo\u0000bar', '0644');
check(fs.chown,       fs.chownSync,       'foo\u0000bar', 12, 34);
check(fs.link,        fs.linkSync,        'foo\u0000bar', 'foobar');
check(fs.link,        fs.linkSync,        'foobar', 'foo\u0000bar');
check(fs.lstat,       fs.lstatSync,       'foo\u0000bar');
check(fs.mkdir,       fs.mkdirSync,       'foo\u0000bar', '0755');
check(fs.open,        fs.openSync,        'foo\u0000bar', 'r');
check(fs.readFile,    fs.readFileSync,    'foo\u0000bar');
check(fs.readdir,     fs.readdirSync,     'foo\u0000bar');
check(fs.readlink,    fs.readlinkSync,    'foo\u0000bar');
check(fs.realpath,    fs.realpathSync,    'foo\u0000bar');
check(fs.rename,      fs.renameSync,      'foo\u0000bar', 'foobar');
check(fs.rename,      fs.renameSync,      'foobar', 'foo\u0000bar');
check(fs.rmdir,       fs.rmdirSync,       'foo\u0000bar');
check(fs.stat,        fs.statSync,        'foo\u0000bar');
check(fs.symlink,     fs.symlinkSync,     'foo\u0000bar', 'foobar');
check(fs.symlink,     fs.symlinkSync,     'foobar', 'foo\u0000bar');
check(fs.truncate,    fs.truncateSync,    'foo\u0000bar');
check(fs.unlink,      fs.unlinkSync,      'foo\u0000bar');
check(null,           fs.unwatchFile,     'foo\u0000bar', common.fail);
check(fs.utimes,      fs.utimesSync,      'foo\u0000bar', 0, 0);
check(null,           fs.watch,           'foo\u0000bar', common.fail);
check(null,           fs.watchFile,       'foo\u0000bar', common.fail);
check(fs.writeFile,   fs.writeFileSync,   'foo\u0000bar');

// an 'error' for exists means that it doesn't exist.
// one of many reasons why this file is the absolute worst.
fs.exists('foo\u0000bar', function(exists) {
  assert(!exists);
});
assert(!fs.existsSync('foo\u0000bar'));
