require("../common");
var fs = require('fs');
var path = require('path');
var async_completed = 0, async_expected = 0, unlink = [];

function asynctest(testBlock, args, callback, assertBlock) {
  async_expected++;
  testBlock.apply(testBlock, args.concat([function(err){
    var ignoreError = false;
    if (assertBlock) {
      try {
        ignoreError = assertBlock.apply(assertBlock, 
          Array.prototype.slice.call(arguments));
      }
      catch (e) {
        err = e;
      }
    }
    async_completed++;
    callback(ignoreError ? null : err);
  }]));
}

function bashRealpath(path, callback) {
  exec("cd '"+path.replace("'","\\'")+"' && pwd -P",function (err, o) {
    callback(err, o.trim());
  });
}

// sub-tests:

function test_simple_relative_symlink(callback) {
  var entry = fixturesDir+'/cycles/symlink',
      expected = fixturesDir+'/cycles/root.js';
  [
    [entry, 'root.js'],
  ].forEach(function(t) {
    try {fs.unlinkSync(t[0]);}catch(e){}
    fs.symlinkSync(t[1], t[0]);
    unlink.push(t[0]);
  });
  var result = fs.realpathSync(entry);
  assert.equal(result, expected,
      'got '+inspect(result)+' expected '+inspect(expected));
  asynctest(fs.realpath, [entry], callback, function(err, result){
    assert.equal(result, expected,
      'got '+inspect(result)+' expected '+inspect(expected));
  });
}

function test_simple_absolute_symlink(callback) {
  bashRealpath(fixturesDir, function(err, fixturesAbsDir) {
    if (err) return callback(err);
    var entry = fixturesAbsDir+'/cycles/symlink',
        expected = fixturesAbsDir+'/nested-index/one/index.js';
    [
      [entry, expected],
    ].forEach(function(t) {
      try {fs.unlinkSync(t[0]);}catch(e){}
      fs.symlinkSync(t[1], t[0]);
      unlink.push(t[0]);
    });
    var result = fs.realpathSync(entry);
    assert.equal(result, expected,
        'got '+inspect(result)+' expected '+inspect(expected));
    asynctest(fs.realpath, [entry], callback, function(err, result){
      assert.equal(result, expected,
        'got '+inspect(result)+' expected '+inspect(expected));
    });
  });
}

function test_deep_relative_file_symlink(callback) {
  var expected = path.join(fixturesDir, 'cycles', 'root.js');
  var linkData1 = "../../cycles/root.js";
  var linkPath1 = path.join(fixturesDir, "nested-index", 'one', 'symlink1.js');
  try {fs.unlinkSync(linkPath1);}catch(e){}
  fs.symlinkSync(linkData1, linkPath1);

  var linkData2 = "../one/symlink1.js";
  var entry = path.join(fixturesDir, "nested-index", 'two', 'symlink1-b.js');
  try {fs.unlinkSync(entry);}catch(e){}
  fs.symlinkSync(linkData2, entry);
  unlink.push(linkPath1);
  unlink.push(entry);

  assert.equal(fs.realpathSync(entry), expected);
  asynctest(fs.realpath, [entry], callback, function(err, result){
    assert.equal(result, expected,
      'got '+inspect(result)+' expected '+inspect(expected));
  });
}

function test_deep_relative_dir_symlink(callback) {
  var expected = path.join(fixturesDir, 'cycles', 'folder');
  var linkData1b = "../../cycles/folder";
  var linkPath1b = path.join(fixturesDir, "nested-index", 'one', 'symlink1-dir');
  try {fs.unlinkSync(linkPath1b);}catch(e){}
  fs.symlinkSync(linkData1b, linkPath1b);

  var linkData2b = "../one/symlink1-dir";
  var entry = path.join(fixturesDir, "nested-index", 'two', 'symlink12-dir');
  try {fs.unlinkSync(entry);}catch(e){}
  fs.symlinkSync(linkData2b, entry);
  unlink.push(linkPath1b);
  unlink.push(entry);

  assert.equal(fs.realpathSync(entry), expected);
  
  asynctest(fs.realpath, [entry], callback, function(err, result){
    assert.equal(result, expected,
      'got '+inspect(result)+' expected '+inspect(expected));
  });
}

function test_cyclic_link_protection(callback) {
  var entry = fixturesDir+'/cycles/realpath-3a';
  [
    [entry, '../cycles/realpath-3b'],
    [fixturesDir+'/cycles/realpath-3b', '../cycles/realpath-3c'],
    [fixturesDir+'/cycles/realpath-3c', '../cycles/realpath-3a'],
  ].forEach(function(t) {
    try {fs.unlinkSync(t[0]);}catch(e){}
    fs.symlinkSync(t[1], t[0]);
    unlink.push(t[0]);
  });
  assert.throws(function(){ fs.realpathSync(entry); });
  asynctest(fs.realpath, [entry], callback, function(err, result){
    assert.ok(err && true);
    return true;
  });
}

function test_relative_input_cwd(callback) {
  var p = fixturesDir.lastIndexOf('/');
  var entrydir = fixturesDir.substr(0, p);
  var entry = fixturesDir.substr(p+1)+'/cycles/realpath-3a';
  var expected = fixturesDir+'/cycles/root.js';
  [
    [entry, '../cycles/realpath-3b'],
    [fixturesDir+'/cycles/realpath-3b', '../cycles/realpath-3c'],
    [fixturesDir+'/cycles/realpath-3c', 'root.js'],
  ].forEach(function(t) {
    var fn = t[0];
    if (fn.charAt(0) !== '/') fn = entrydir + '/' + fn;
    try {fs.unlinkSync(fn);}catch(e){}
    fs.symlinkSync(t[1], fn);
    unlink.push(fn);
  });
  var origcwd = process.cwd();
  process.chdir(entrydir);
  assert.equal(fs.realpathSync(entry), expected);
  asynctest(fs.realpath, [entry], callback, function(err, result){
    process.chdir(origcwd);
    assert.equal(result, expected,
      'got '+inspect(result)+' expected '+inspect(expected));
    return true;
  });
}

function test_deep_symlink_mix(callback) {
  // todo: check to see that fixturesDir is not rooted in the
  //       same directory as our test symlink.
  // obtain our current realpath using bash (so we can test ourselves)
  bashRealpath(fixturesDir, function(err, fixturesAbsDir) {
    if (err) return callback(err);
    /*
    /tmp/node-test-realpath-f1 -> ../tmp/node-test-realpath-d1/foo
    /tmp/node-test-realpath-d1 -> ../node-test-realpath-d2
    /tmp/node-test-realpath-d2/foo -> ../node-test-realpath-f2
    /tmp/node-test-realpath-f2 
      -> /node/test/fixtures/nested-index/one/realpath-c
    /node/test/fixtures/nested-index/one/realpath-c 
      -> /node/test/fixtures/nested-index/two/realpath-c
    /node/test/fixtures/nested-index/two/realpath-c -> ../../cycles/root.js
    /node/test/fixtures/cycles/root.js (hard)
    */
    var entry = '/tmp/node-test-realpath-f1';
    try {fs.unlinkSync('/tmp/node-test-realpath-d2/foo');}catch(e){}
    try {fs.rmdirSync('/tmp/node-test-realpath-d2');}catch(e){}
    fs.mkdirSync('/tmp/node-test-realpath-d2', 0700);
    try {
      [
        [entry, '../tmp/node-test-realpath-d1/foo'],
        ['/tmp/node-test-realpath-d1', '../tmp/node-test-realpath-d2'],
        ['/tmp/node-test-realpath-d2/foo', '../node-test-realpath-f2'],
        ['/tmp/node-test-realpath-f2', fixturesAbsDir+'/nested-index/one/realpath-c'],
        [fixturesAbsDir+'/nested-index/one/realpath-c', fixturesAbsDir+'/nested-index/two/realpath-c'],
        [fixturesAbsDir+'/nested-index/two/realpath-c', '../../cycles/root.js'],
      ].forEach(function(t) {
        //debug('setting up '+t[0]+' -> '+t[1]);
        try {fs.unlinkSync(t[0]);}catch(e){}
        fs.symlinkSync(t[1], t[0]);
        unlink.push(t[0]);
      });
    } finally {
      unlink.push('/tmp/node-test-realpath-d2');
    }
    var expected = fixturesAbsDir+'/cycles/root.js';
    assert.equal(fs.realpathSync(entry), expected);
    asynctest(fs.realpath, [entry], callback, function(err, result){
      assert.equal(result, expected,
        'got '+inspect(result)+' expected '+inspect(expected));
      return true;
    });
  });
}

function test_non_symlinks(callback) {
  bashRealpath(fixturesDir, function(err, fixturesAbsDir) {
    if (err) return callback(err);
    var p = fixturesAbsDir.lastIndexOf('/');
    var entrydir = fixturesAbsDir.substr(0, p);
    var entry = fixturesAbsDir.substr(p+1)+'/cycles/root.js';
    var expected = fixturesAbsDir+'/cycles/root.js';
    var origcwd = process.cwd();
    process.chdir(entrydir);
    assert.equal(fs.realpathSync(entry), expected);
    asynctest(fs.realpath, [entry], callback, function(err, result){
      process.chdir(origcwd);
      assert.equal(result, expected,
        'got '+inspect(result)+' expected '+inspect(expected));
      return true;
    });
  });
}

// ----------------------------------------------------------------------------

var tests = [
  test_simple_relative_symlink,
  test_simple_absolute_symlink,
  test_deep_relative_file_symlink,
  test_deep_relative_dir_symlink,
  test_cyclic_link_protection,
  test_relative_input_cwd,
  test_deep_symlink_mix,
  test_non_symlinks,
];
var numtests = tests.length;
function runNextTest(err) {
  if (err) throw err;
  var test = tests.shift()
  if (!test) console.log(numtests+' subtests completed OK for fs.realpath');
  else test(runNextTest);
}
runNextTest();

process.addListener("exit", function () {
  unlink.forEach(function(path){ try {fs.unlinkSync(path);}catch(e){} });
  assert.equal(async_completed, async_expected);
});
