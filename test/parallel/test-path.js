'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');

const f = __filename;
const failures = [];

// path.basename tests
assert.equal(path.basename(f), 'test-path.js');
assert.equal(path.basename(f, '.js'), 'test-path');
assert.equal(path.basename(''), '');
assert.equal(path.basename('/dir/basename.ext'), 'basename.ext');
assert.equal(path.basename('/basename.ext'), 'basename.ext');
assert.equal(path.basename('basename.ext'), 'basename.ext');
assert.equal(path.basename('basename.ext/'), 'basename.ext');
assert.equal(path.basename('basename.ext//'), 'basename.ext');

// On Windows a backslash acts as a path separator.
assert.equal(path.win32.basename('\\dir\\basename.ext'), 'basename.ext');
assert.equal(path.win32.basename('\\basename.ext'), 'basename.ext');
assert.equal(path.win32.basename('basename.ext'), 'basename.ext');
assert.equal(path.win32.basename('basename.ext\\'), 'basename.ext');
assert.equal(path.win32.basename('basename.ext\\\\'), 'basename.ext');
assert.equal(path.win32.basename('foo'), 'foo');
assert.equal(path.win32.basename(null), 'null');
assert.equal(path.win32.basename(true), 'true');
assert.equal(path.win32.basename(1), '1');
assert.equal(path.win32.basename(), 'undefined');
assert.equal(path.win32.basename({}), '[object Object]');

// On unix a backslash is just treated as any other character.
assert.equal(path.posix.basename('\\dir\\basename.ext'), '\\dir\\basename.ext');
assert.equal(path.posix.basename('\\basename.ext'), '\\basename.ext');
assert.equal(path.posix.basename('basename.ext'), 'basename.ext');
assert.equal(path.posix.basename('basename.ext\\'), 'basename.ext\\');
assert.equal(path.posix.basename('basename.ext\\\\'), 'basename.ext\\\\');
assert.equal(path.posix.basename('foo'), 'foo');
assert.equal(path.posix.basename(null), 'null');
assert.equal(path.posix.basename(true), 'true');
assert.equal(path.posix.basename(1), '1');
assert.equal(path.posix.basename(), 'undefined');
assert.equal(path.posix.basename({}), '[object Object]');

// POSIX filenames may include control characters
// c.f. http://www.dwheeler.com/essays/fixing-unix-linux-filenames.html
const controlCharFilename = 'Icon' + String.fromCharCode(13);
assert.equal(path.posix.basename('/a/b/' + controlCharFilename),
             controlCharFilename);


// path.dirname tests
assert.equal(path.dirname(f).substr(-13),
             common.isWindows ? 'test\\parallel' : 'test/parallel');

assert.equal(path.posix.dirname('/a/b/'), '/a');
assert.equal(path.posix.dirname('/a/b'), '/a');
assert.equal(path.posix.dirname('/a'), '/');
assert.equal(path.posix.dirname(''), '.');
assert.equal(path.posix.dirname('/'), '/');
assert.equal(path.posix.dirname('////'), '/');
assert.equal(path.posix.dirname('foo'), '.');
assert.equal(path.posix.dirname(null), '.');
assert.equal(path.posix.dirname(true), '.');
assert.equal(path.posix.dirname(1), '.');
assert.equal(path.posix.dirname(), '.');
assert.equal(path.posix.dirname({}), '.');

assert.equal(path.win32.dirname('c:\\'), 'c:\\');
assert.equal(path.win32.dirname('c:\\foo'), 'c:\\');
assert.equal(path.win32.dirname('c:\\foo\\'), 'c:\\');
assert.equal(path.win32.dirname('c:\\foo\\bar'), 'c:\\foo');
assert.equal(path.win32.dirname('c:\\foo\\bar\\'), 'c:\\foo');
assert.equal(path.win32.dirname('c:\\foo\\bar\\baz'), 'c:\\foo\\bar');
assert.equal(path.win32.dirname('\\'), '\\');
assert.equal(path.win32.dirname('\\foo'), '\\');
assert.equal(path.win32.dirname('\\foo\\'), '\\');
assert.equal(path.win32.dirname('\\foo\\bar'), '\\foo');
assert.equal(path.win32.dirname('\\foo\\bar\\'), '\\foo');
assert.equal(path.win32.dirname('\\foo\\bar\\baz'), '\\foo\\bar');
assert.equal(path.win32.dirname('c:'), 'c:');
assert.equal(path.win32.dirname('c:foo'), 'c:');
assert.equal(path.win32.dirname('c:foo\\'), 'c:');
assert.equal(path.win32.dirname('c:foo\\bar'), 'c:foo');
assert.equal(path.win32.dirname('c:foo\\bar\\'), 'c:foo');
assert.equal(path.win32.dirname('c:foo\\bar\\baz'), 'c:foo\\bar');
assert.equal(path.win32.dirname('\\\\unc\\share'), '\\\\unc\\share');
assert.equal(path.win32.dirname('\\\\unc\\share\\foo'), '\\\\unc\\share\\');
assert.equal(path.win32.dirname('\\\\unc\\share\\foo\\'), '\\\\unc\\share\\');
assert.equal(path.win32.dirname('\\\\unc\\share\\foo\\bar'),
             '\\\\unc\\share\\foo');
assert.equal(path.win32.dirname('\\\\unc\\share\\foo\\bar\\'),
             '\\\\unc\\share\\foo');
assert.equal(path.win32.dirname('\\\\unc\\share\\foo\\bar\\baz'),
             '\\\\unc\\share\\foo\\bar');
assert.equal(path.win32.dirname('/a/b/'), '/a');
assert.equal(path.win32.dirname('/a/b'), '/a');
assert.equal(path.win32.dirname('/a'), '/');
assert.equal(path.win32.dirname(''), '.');
assert.equal(path.win32.dirname('/'), '/');
assert.equal(path.win32.dirname('////'), '/');
assert.equal(path.win32.dirname('foo'), '.');
assert.equal(path.win32.dirname(null), '.');
assert.equal(path.win32.dirname(true), '.');
assert.equal(path.win32.dirname(1), '.');
assert.equal(path.win32.dirname(), '.');
assert.equal(path.win32.dirname({}), '.');


// path.extname tests
[
  [f, '.js'],
  ['', ''],
  ['/path/to/file', ''],
  ['/path/to/file.ext', '.ext'],
  ['/path.to/file.ext', '.ext'],
  ['/path.to/file', ''],
  ['/path.to/.file', ''],
  ['/path.to/.file.ext', '.ext'],
  ['/path/to/f.ext', '.ext'],
  ['/path/to/..ext', '.ext'],
  ['/path/to/..', ''],
  ['file', ''],
  ['file.ext', '.ext'],
  ['.file', ''],
  ['.file.ext', '.ext'],
  ['/file', ''],
  ['/file.ext', '.ext'],
  ['/.file', ''],
  ['/.file.ext', '.ext'],
  ['.path/file.ext', '.ext'],
  ['file.ext.ext', '.ext'],
  ['file.', '.'],
  ['.', ''],
  ['./', ''],
  ['.file.ext', '.ext'],
  ['.file', ''],
  ['.file.', '.'],
  ['.file..', '.'],
  ['..', ''],
  ['../', ''],
  ['..file.ext', '.ext'],
  ['..file', '.file'],
  ['..file.', '.'],
  ['..file..', '.'],
  ['...', '.'],
  ['...ext', '.ext'],
  ['....', '.'],
  ['file.ext/', '.ext'],
  ['file.ext//', '.ext'],
  ['file/', ''],
  ['file//', ''],
  ['file./', '.'],
  ['file.//', '.'],
].forEach(function(test) {
  [path.posix.extname, path.win32.extname].forEach(function(extname) {
    let input = test[0];
    if (extname === path.win32.extname)
      input = input.replace(/\//g, '\\');
    const actual = extname(input);
    const expected = test[1];
    const fn = 'path.' +
               (extname === path.win32.extname ? 'win32' : 'posix') +
               '.extname(';
    const message = fn + JSON.stringify(input) + ')' +
                    '\n  expect=' + JSON.stringify(expected) +
                    '\n  actual=' + JSON.stringify(actual);
    if (actual !== expected)
      failures.push('\n' + message);
  });
});
assert.equal(failures.length, 0, failures.join(''));

// On Windows, backslash is a path separator.
assert.equal(path.win32.extname('.\\'), '');
assert.equal(path.win32.extname('..\\'), '');
assert.equal(path.win32.extname('file.ext\\'), '.ext');
assert.equal(path.win32.extname('file.ext\\\\'), '.ext');
assert.equal(path.win32.extname('file\\'), '');
assert.equal(path.win32.extname('file\\\\'), '');
assert.equal(path.win32.extname('file.\\'), '.');
assert.equal(path.win32.extname('file.\\\\'), '.');
assert.equal(path.win32.extname(null), '');
assert.equal(path.win32.extname(true), '');
assert.equal(path.win32.extname(1), '');
assert.equal(path.win32.extname(), '');
assert.equal(path.win32.extname({}), '');

// On *nix, backslash is a valid name component like any other character.
assert.equal(path.posix.extname('.\\'), '');
assert.equal(path.posix.extname('..\\'), '.\\');
assert.equal(path.posix.extname('file.ext\\'), '.ext\\');
assert.equal(path.posix.extname('file.ext\\\\'), '.ext\\\\');
assert.equal(path.posix.extname('file\\'), '');
assert.equal(path.posix.extname('file\\\\'), '');
assert.equal(path.posix.extname('file.\\'), '.\\');
assert.equal(path.posix.extname('file.\\\\'), '.\\\\');
assert.equal(path.posix.extname(null), '');
assert.equal(path.posix.extname(true), '');
assert.equal(path.posix.extname(1), '');
assert.equal(path.posix.extname(), '');
assert.equal(path.posix.extname({}), '');


// path.join tests
const joinTests = [
  [ [path.posix.join, path.win32.join],
    // arguments                     result
    [[['.', 'x/b', '..', '/b/c.js'], 'x/b/c.js'],
     [['/.', 'x/b', '..', '/b/c.js'], '/x/b/c.js'],
     [['/foo', '../../../bar'], '/bar'],
     [['foo', '../../../bar'], '../../bar'],
     [['foo/', '../../../bar'], '../../bar'],
     [['foo/x', '../../../bar'], '../bar'],
     [['foo/x', './bar'], 'foo/x/bar'],
     [['foo/x/', './bar'], 'foo/x/bar'],
     [['foo/x/', '.', 'bar'], 'foo/x/bar'],
     [['./'], './'],
     [['.', './'], './'],
     [['.', '.', '.'], '.'],
     [['.', './', '.'], '.'],
     [['.', '/./', '.'], '.'],
     [['.', '/////./', '.'], '.'],
     [['.'], '.'],
     [['', '.'], '.'],
     [['', 'foo'], 'foo'],
     [['foo', '/bar'], 'foo/bar'],
     [['', '/foo'], '/foo'],
     [['', '', '/foo'], '/foo'],
     [['', '', 'foo'], 'foo'],
     [['foo', ''], 'foo'],
     [['foo/', ''], 'foo/'],
     [['foo', '', '/bar'], 'foo/bar'],
     [['./', '..', '/foo'], '../foo'],
     [['./', '..', '..', '/foo'], '../../foo'],
     [['.', '..', '..', '/foo'], '../../foo'],
     [['', '..', '..', '/foo'], '../../foo'],
     [['/'], '/'],
     [['/', '.'], '/'],
     [['/', '..'], '/'],
     [['/', '..', '..'], '/'],
     [[''], '.'],
     [['', ''], '.'],
     [[' /foo'], ' /foo'],
     [[' ', 'foo'], ' /foo'],
     [[' ', '.'], ' '],
     [[' ', '/'], ' /'],
     [[' ', ''], ' '],
     [['/', 'foo'], '/foo'],
     [['/', '/foo'], '/foo'],
     [['/', '//foo'], '/foo'],
     [['/', '', '/foo'], '/foo'],
     [['', '/', 'foo'], '/foo'],
     [['', '/', '/foo'], '/foo']
    ]
  ]
];

// Windows-specific join tests
joinTests.push([
  path.win32.join,
  joinTests[0][1].slice(0).concat(
    [// arguments                     result
     // UNC path expected
     [['//foo/bar'], '\\\\foo\\bar\\'],
     [['\\/foo/bar'], '\\\\foo\\bar\\'],
     [['\\\\foo/bar'], '\\\\foo\\bar\\'],
     // UNC path expected - server and share separate
     [['//foo', 'bar'], '\\\\foo\\bar\\'],
     [['//foo/', 'bar'], '\\\\foo\\bar\\'],
     [['//foo', '/bar'], '\\\\foo\\bar\\'],
     // UNC path expected - questionable
     [['//foo', '', 'bar'], '\\\\foo\\bar\\'],
     [['//foo/', '', 'bar'], '\\\\foo\\bar\\'],
     [['//foo/', '', '/bar'], '\\\\foo\\bar\\'],
     // UNC path expected - even more questionable
     [['', '//foo', 'bar'], '\\\\foo\\bar\\'],
     [['', '//foo/', 'bar'], '\\\\foo\\bar\\'],
     [['', '//foo/', '/bar'], '\\\\foo\\bar\\'],
     // No UNC path expected (no double slash in first component)
     [['\\', 'foo/bar'], '\\foo\\bar'],
     [['\\', '/foo/bar'], '\\foo\\bar'],
     [['', '/', '/foo/bar'], '\\foo\\bar'],
     // No UNC path expected (no non-slashes in first component -
     // questionable)
     [['//', 'foo/bar'], '\\foo\\bar'],
     [['//', '/foo/bar'], '\\foo\\bar'],
     [['\\\\', '/', '/foo/bar'], '\\foo\\bar'],
     [['//'], '/'],
     // No UNC path expected (share name missing - questionable).
     [['//foo'], '\\foo'],
     [['//foo/'], '\\foo\\'],
     [['//foo', '/'], '\\foo\\'],
     [['//foo', '', '/'], '\\foo\\'],
     // No UNC path expected (too many leading slashes - questionable)
     [['///foo/bar'], '\\foo\\bar'],
     [['////foo', 'bar'], '\\foo\\bar'],
     [['\\\\\\/foo/bar'], '\\foo\\bar'],
     // Drive-relative vs drive-absolute paths. This merely describes the
     // status quo, rather than being obviously right
     [['c:'], 'c:.'],
     [['c:.'], 'c:.'],
     [['c:', ''], 'c:.'],
     [['', 'c:'], 'c:.'],
     [['c:.', '/'], 'c:.\\'],
     [['c:.', 'file'], 'c:file'],
     [['c:', '/'], 'c:\\'],
     [['c:', 'file'], 'c:\\file']
    ]
  )
]);
joinTests.forEach(function(test) {
  if (!Array.isArray(test[0]))
    test[0] = [test[0]];
  test[0].forEach(function(join) {
    test[1].forEach(function(test) {
      const actual = join.apply(null, test[0]);
      const expected = test[1];
      // For non-Windows specific tests with the Windows join(), we need to try
      // replacing the slashes since the non-Windows specific tests' `expected`
      // use forward slashes
      const actualAlt = (join === path.win32.join) ?
        actual.replace(/\\/g, '/') : undefined;
      const fn = 'path.' +
                 (join === path.win32.join ? 'win32' : 'posix') +
                 '.join(';
      const message = fn + test[0].map(JSON.stringify).join(',') + ')' +
                      '\n  expect=' + JSON.stringify(expected) +
                      '\n  actual=' + JSON.stringify(actual);
      if (actual !== expected && actualAlt !== expected)
        failures.push('\n' + message);
    });
  });
});
assert.equal(failures.length, 0, failures.join(''));


// Test thrown TypeErrors
var typeErrorTests = [true, false, 7, null, {}, undefined, [], NaN];

function fail(fn) {
  var args = Array.prototype.slice.call(arguments, 1);

  assert.throws(function() {
    fn.apply(null, args);
  }, TypeError);
}

typeErrorTests.forEach(function(test) {
  fail(path.join, test);
  fail(path.resolve, test);
  fail(path.normalize, test);
  fail(path.isAbsolute, test);
  fail(path.relative, test, 'foo');
  fail(path.relative, 'foo', test);
  fail(path.parse, test);

  // These methods should throw a TypeError, but do not for backwards
  // compatibility. Uncommenting these lines in the future should be a goal.
  // fail(path.dirname, test);
  // fail(path.basename, test);
  // fail(path.extname, test);

  // undefined is a valid value as the second argument to basename
  if (test !== undefined) {
    fail(path.basename, 'foo', test);
  }
});


// path.normalize tests
assert.equal(path.win32.normalize('./fixtures///b/../b/c.js'),
             'fixtures\\b\\c.js');
assert.equal(path.win32.normalize('/foo/../../../bar'), '\\bar');
assert.equal(path.win32.normalize('a//b//../b'), 'a\\b');
assert.equal(path.win32.normalize('a//b//./c'), 'a\\b\\c');
assert.equal(path.win32.normalize('a//b//.'), 'a\\b');
assert.equal(path.win32.normalize('//server/share/dir/file.ext'),
             '\\\\server\\share\\dir\\file.ext');
assert.equal(path.win32.normalize('/a/b/c/../../../x/y/z'), '\\x\\y\\z');

assert.equal(path.posix.normalize('./fixtures///b/../b/c.js'),
             'fixtures/b/c.js');
assert.equal(path.posix.normalize('/foo/../../../bar'), '/bar');
assert.equal(path.posix.normalize('a//b//../b'), 'a/b');
assert.equal(path.posix.normalize('a//b//./c'), 'a/b/c');
assert.equal(path.posix.normalize('a//b//.'), 'a/b');
assert.equal(path.posix.normalize('/a/b/c/../../../x/y/z'), '/x/y/z');
assert.equal(path.posix.normalize('///..//./foo/.//bar'), '/foo/bar');


// path.resolve tests
const resolveTests = [
  [ path.win32.resolve,
    // arguments                                    result
    [[['c:/blah\\blah', 'd:/games', 'c:../a'], 'c:\\blah\\a'],
     [['c:/ignore', 'd:\\a/b\\c/d', '\\e.exe'], 'd:\\e.exe'],
     [['c:/ignore', 'c:/some/file'], 'c:\\some\\file'],
     [['d:/ignore', 'd:some/dir//'], 'd:\\ignore\\some\\dir'],
     [['.'], process.cwd()],
     [['//server/share', '..', 'relative\\'], '\\\\server\\share\\relative'],
     [['c:/', '//'], 'c:\\'],
     [['c:/', '//dir'], 'c:\\dir'],
     [['c:/', '//server/share'], '\\\\server\\share\\'],
     [['c:/', '//server//share'], '\\\\server\\share\\'],
     [['c:/', '///some//dir'], 'c:\\some\\dir'],
     [['C:\\foo\\tmp.3\\', '..\\tmp.3\\cycles\\root.js'],
      'C:\\foo\\tmp.3\\cycles\\root.js']
    ]
  ],
  [ path.posix.resolve,
    // arguments                                    result
    [[['/var/lib', '../', 'file/'], '/var/file'],
     [['/var/lib', '/../', 'file/'], '/file'],
     [['a/b/c/', '../../..'], process.cwd()],
     [['.'], process.cwd()],
     [['/some/dir', '.', '/absolute/'], '/absolute'],
     [['/foo/tmp.3/', '../tmp.3/cycles/root.js'], '/foo/tmp.3/cycles/root.js']
    ]
  ]
];
resolveTests.forEach(function(test) {
  const resolve = test[0];
  test[1].forEach(function(test) {
    const actual = resolve.apply(null, test[0]);
    let actualAlt;
    if (resolve === path.win32.resolve && !common.isWindows)
      actualAlt = actual.replace(/\\/g, '/');
    else if (resolve !== path.win32.resolve && common.isWindows)
      actualAlt = actual.replace(/\//g, '\\');
    const expected = test[1];
    const fn = 'path.' +
               (resolve === path.win32.resolve ? 'win32' : 'posix') +
               '.resolve(';
    const message = fn + test[0].map(JSON.stringify).join(',') + ')' +
                    '\n  expect=' + JSON.stringify(expected) +
                    '\n  actual=' + JSON.stringify(actual);
    if (actual !== expected && actualAlt !== expected)
      failures.push('\n' + message);
  });
});
assert.equal(failures.length, 0, failures.join(''));


// path.isAbsolute tests
assert.equal(path.win32.isAbsolute('//server/file'), true);
assert.equal(path.win32.isAbsolute('\\\\server\\file'), true);
assert.equal(path.win32.isAbsolute('C:/Users/'), true);
assert.equal(path.win32.isAbsolute('C:\\Users\\'), true);
assert.equal(path.win32.isAbsolute('C:cwd/another'), false);
assert.equal(path.win32.isAbsolute('C:cwd\\another'), false);
assert.equal(path.win32.isAbsolute('directory/directory'), false);
assert.equal(path.win32.isAbsolute('directory\\directory'), false);

assert.equal(path.posix.isAbsolute('/home/foo'), true);
assert.equal(path.posix.isAbsolute('/home/foo/..'), true);
assert.equal(path.posix.isAbsolute('bar/'), false);
assert.equal(path.posix.isAbsolute('./baz'), false);


// path.relative tests
const relativeTests = [
  [ path.win32.relative,
    // arguments                     result
    [['c:/blah\\blah', 'd:/games', 'd:\\games'],
     ['c:/aaaa/bbbb', 'c:/aaaa', '..'],
     ['c:/aaaa/bbbb', 'c:/cccc', '..\\..\\cccc'],
     ['c:/aaaa/bbbb', 'c:/aaaa/bbbb', ''],
     ['c:/aaaa/bbbb', 'c:/aaaa/cccc', '..\\cccc'],
     ['c:/aaaa/', 'c:/aaaa/cccc', 'cccc'],
     ['c:/', 'c:\\aaaa\\bbbb', 'aaaa\\bbbb'],
     ['c:/aaaa/bbbb', 'd:\\', 'd:\\'],
     ['c:/AaAa/bbbb', 'c:/aaaa/bbbb', ''],
     ['c:/aaaaa/', 'c:/aaaa/cccc', '..\\aaaa\\cccc'],
     ['C:\\foo\\bar\\baz\\quux', 'C:\\', '..\\..\\..\\..'],
     ['C:\\foo\\test', 'C:\\foo\\test\\bar\\package.json', 'bar\\package.json'],
     ['C:\\foo\\bar\\baz-quux', 'C:\\foo\\bar\\baz', '..\\baz'],
     ['C:\\foo\\bar\\baz', 'C:\\foo\\bar\\baz-quux', '..\\baz-quux'],
     ['\\\\foo\\bar', '\\\\foo\\bar\\baz', 'baz'],
     ['\\\\foo\\bar\\baz', '\\\\foo\\bar', '..'],
     ['\\\\foo\\bar\\baz-quux', '\\\\foo\\bar\\baz', '..\\baz'],
     ['\\\\foo\\bar\\baz', '\\\\foo\\bar\\baz-quux', '..\\baz-quux'],
     ['C:\\baz-quux', 'C:\\baz', '..\\baz'],
     ['C:\\baz', 'C:\\baz-quux', '..\\baz-quux'],
     ['\\\\foo\\baz-quux', '\\\\foo\\baz', '..\\baz'],
     ['\\\\foo\\baz', '\\\\foo\\baz-quux', '..\\baz-quux']
    ]
  ],
  [ path.posix.relative,
    // arguments                    result
    [['/var/lib', '/var', '..'],
     ['/var/lib', '/bin', '../../bin'],
     ['/var/lib', '/var/lib', ''],
     ['/var/lib', '/var/apache', '../apache'],
     ['/var/', '/var/lib', 'lib'],
     ['/', '/var/lib', 'var/lib'],
     ['/foo/test', '/foo/test/bar/package.json', 'bar/package.json'],
     ['/Users/a/web/b/test/mails', '/Users/a/web/b', '../..'],
     ['/foo/bar/baz-quux', '/foo/bar/baz', '../baz'],
     ['/foo/bar/baz', '/foo/bar/baz-quux', '../baz-quux'],
     ['/baz-quux', '/baz', '../baz'],
     ['/baz', '/baz-quux', '../baz-quux']
    ]
  ]
];
relativeTests.forEach(function(test) {
  const relative = test[0];
  test[1].forEach(function(test) {
    const actual = relative(test[0], test[1]);
    const expected = test[2];
    const fn = 'path.' +
               (relative === path.win32.relative ? 'win32' : 'posix') +
               '.relative(';
    const message = fn +
                    test.slice(0, 2).map(JSON.stringify).join(',') +
                    ')' +
                    '\n  expect=' + JSON.stringify(expected) +
                    '\n  actual=' + JSON.stringify(actual);
    if (actual !== expected)
      failures.push('\n' + message);
  });
});
assert.equal(failures.length, 0, failures.join(''));


// path.sep tests
// windows
assert.equal(path.win32.sep, '\\');
// posix
assert.equal(path.posix.sep, '/');

// path.delimiter tests
// windows
assert.equal(path.win32.delimiter, ';');
// posix
assert.equal(path.posix.delimiter, ':');


// path._makeLong tests
const emptyObj = {};
assert.equal(path.posix._makeLong('/foo/bar'), '/foo/bar');
assert.equal(path.posix._makeLong('foo/bar'), 'foo/bar');
assert.equal(path.posix._makeLong(null), null);
assert.equal(path.posix._makeLong(true), true);
assert.equal(path.posix._makeLong(1), 1);
assert.equal(path.posix._makeLong(), undefined);
assert.equal(path.posix._makeLong(emptyObj), emptyObj);
if (common.isWindows) {
  // These tests cause resolve() to insert the cwd, so we cannot test them from
  // non-Windows platforms (easily)
  assert.equal(path.win32._makeLong('foo\\bar').toLowerCase(),
               '\\\\?\\' + process.cwd().toLowerCase() + '\\foo\\bar');
  assert.equal(path.win32._makeLong('foo/bar').toLowerCase(),
               '\\\\?\\' + process.cwd().toLowerCase() + '\\foo\\bar');
  assert.equal(path.win32._makeLong('C:').toLowerCase(),
               '\\\\?\\' + process.cwd().toLowerCase());
  assert.equal(path.win32._makeLong('C').toLowerCase(),
               '\\\\?\\' + process.cwd().toLowerCase() + '\\c');
}
assert.equal(path.win32._makeLong('C:\\foo'), '\\\\?\\C:\\foo');
assert.equal(path.win32._makeLong('C:/foo'), '\\\\?\\C:\\foo');
assert.equal(path.win32._makeLong('\\\\foo\\bar'), '\\\\?\\UNC\\foo\\bar\\');
assert.equal(path.win32._makeLong('//foo//bar'), '\\\\?\\UNC\\foo\\bar\\');
assert.equal(path.win32._makeLong('\\\\?\\foo'), '\\\\?\\foo');
assert.equal(path.win32._makeLong(null), null);
assert.equal(path.win32._makeLong(true), true);
assert.equal(path.win32._makeLong(1), 1);
assert.equal(path.win32._makeLong(), undefined);
assert.equal(path.win32._makeLong(emptyObj), emptyObj);


if (common.isWindows)
  assert.deepEqual(path, path.win32, 'should be win32 path module');
else
  assert.deepEqual(path, path.posix, 'should be posix path module');
