var common = require('../common');
var assert = require('assert');

var path = require('path');

var isWindows = process.platform === 'win32';

var f = __filename;

assert.equal(path.basename(f), 'test-path.js');
assert.equal(path.basename(f, '.js'), 'test-path');
assert.equal(path.extname(f), '.js');
assert.equal(path.dirname(f).substr(-11), isWindows ? 'test\\simple' : 'test/simple');
assert.equal(path.dirname('/a/b/'), '/a');
assert.equal(path.dirname('/a/b'), '/a');
assert.equal(path.dirname('/a'), '/');
assert.equal(path.dirname('/'), '/');
path.exists(f, function(y) { assert.equal(y, true) });

assert.equal(path.existsSync(f), true);

assert.equal(path.extname(''), '');
assert.equal(path.extname('/path/to/file'), '');
assert.equal(path.extname('/path/to/file.ext'), '.ext');
assert.equal(path.extname('/path.to/file.ext'), '.ext');
assert.equal(path.extname('/path.to/file'), '');
assert.equal(path.extname('/path.to/.file'), '');
assert.equal(path.extname('/path.to/.file.ext'), '.ext');
assert.equal(path.extname('/path/to/f.ext'), '.ext');
assert.equal(path.extname('/path/to/..ext'), '.ext');
assert.equal(path.extname('file'), '');
assert.equal(path.extname('file.ext'), '.ext');
assert.equal(path.extname('.file'), '');
assert.equal(path.extname('.file.ext'), '.ext');
assert.equal(path.extname('/file'), '');
assert.equal(path.extname('/file.ext'), '.ext');
assert.equal(path.extname('/.file'), '');
assert.equal(path.extname('/.file.ext'), '.ext');
assert.equal(path.extname('.path/file.ext'), '.ext');
assert.equal(path.extname('file.ext.ext'), '.ext');
assert.equal(path.extname('file.'), '.');

// path.join tests
var failures = [];
var joinTests =
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
     // filtration of non-strings.
     [['x', true, 7, 'y', null, {}], 'x/y']
    ];
joinTests.forEach(function(test) {
  var actual = path.join.apply(path, test[0]);
  var expected = isWindows ? test[1].replace(/\//g, '\\') : test[1];
  var message = 'path.join(' + test[0].map(JSON.stringify).join(',') + ')' +
                '\n  expect=' + JSON.stringify(expected) +
                '\n  actual=' + JSON.stringify(actual);
  if (actual !== expected) failures.push('\n' + message);
  // assert.equal(actual, expected, message);
});
assert.equal(failures.length, 0, failures.join(''));

// path normalize tests
if (isWindows) {
  assert.equal(path.normalize('./fixtures///b/../b/c.js'),
               'fixtures\\b\\c.js');
  assert.equal(path.normalize('/foo/../../../bar'), '\\bar');
  assert.equal(path.normalize('a//b//../b'), 'a\\b');
  assert.equal(path.normalize('a//b//./c'), 'a\\b\\c');
  assert.equal(path.normalize('a//b//.'), 'a\\b');
} else {
  assert.equal(path.normalize('./fixtures///b/../b/c.js'),
               'fixtures/b/c.js');
  assert.equal(path.normalize('/foo/../../../bar'), '/bar');
  assert.equal(path.normalize('a//b//../b'), 'a/b');
  assert.equal(path.normalize('a//b//./c'), 'a/b/c');
  assert.equal(path.normalize('a//b//.'), 'a/b');
}

// path.resolve tests
if (isWindows) {
  // windows
  var resolveTests =
    // arguments                                    result
    [[['c:/blah\\blah', 'd:/games', 'c:../a'     ], 'c:\\blah\\a'    ],
     [['c:/ignore', 'd:\\a/b\\c/d', '\\e.exe'    ], 'd:\\e.exe'      ],
     [['c:/ignore', 'c:/some/file'               ], 'c:\\some\\file' ],
     [['d:/ignore', 'd:some/dir//'               ], 'd:\\ignore\\some\\dir' ],
     [['.'                                       ], process.cwd()    ],
     [['//server/share', '..', 'relative\\'      ], '\\\\server\\share\\relative' ]];
} else {
  // Posix
  var resolveTests =
    // arguments                                    result
    [[['/var/lib', '../', 'file/'                ], '/var/file'      ],
     [['/var/lib', '/../', 'file/'               ], '/file'          ],
     [['a/b/c/', '../../..'                      ], process.cwd()    ],
     [['.'                                       ], process.cwd()    ],
     [['/some/dir', '.', '/absolute/'            ], '/absolute'      ]];
}
var failures = []
resolveTests.forEach(function(test) {
  var actual = path.resolve.apply(path, test[0]);
  var expected = test[1];
  var message = 'path.resolve(' + test[0].map(JSON.stringify).join(',') + ')' +
                '\n  expect=' + JSON.stringify(expected) +
                '\n  actual=' + JSON.stringify(actual);
  if (actual !== expected) failures.push('\n' + message);
  // assert.equal(actual, expected, message);
});
assert.equal(failures.length, 0, failures.join(''));
