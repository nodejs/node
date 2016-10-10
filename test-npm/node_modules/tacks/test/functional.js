var fs = require('fs')
var path = require('path')
var tap = require('tap')
var test = require('tap').test
var rimraf = require('rimraf')
var loadFromDir = require('../load-from-dir.js')
var generateFromDir = require('../generate-from-dir.js')
var tacksAreTheSame = require('../tap.js').areTheSame
var Tacks = require('../index.js')
var File = Tacks.File
var Dir = Tacks.Dir
var Symlink = Tacks.Symlink

var testroot = path.join(__dirname, path.basename(__filename, '.js'))
var testdir = path.join(testroot, 'example')
var testmodule = path.join(testroot, 'example.js')

var fixture = new Tacks(
  Dir({
    'a': Dir({
      'b': Dir({
        'c': Dir({
          'foo.txt': File(''),
          'bar.txt': Symlink('foo.txt'),
          'ascii.txt': Symlink('/ascii.txt')
        })
      })
    }),
    'ascii.txt': File(
      'abc\n'
    ),
    'foo': Dir({
      'foo.txt': Symlink('/a/b/c/foo.txt')
    }),
    'binary.gz': File(new Buffer(
      '1f8b0800d063115700034b4c4ae602004e81884704000000',
      'hex'
    )),
    'empty.txt': File(''),
    'example.json': File({
      'a': true,
      'b': 23,
      'c': 'xyzzy',
      'd': [],
      'e': {},
      'f': null,
      'complex': {
        'xyz': [1, 2, 3, { abc: 'def' }],
        '123': false
      }
    }),
    'x': Dir({
      'y': Dir({
        'z': Dir({
        })
      })
    })
  })
)

function setup () {
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

test('setup', function (t) {
  rimraf.sync(testroot)
  setup()
  t.done()
})

test('loadFromDir', function (t) {
  var model = loadFromDir(testdir)
  return tacksAreTheSame(t, model, fixture, 'loadFromDir')
})

test('generateFromDir', function (t) {
  var js = generateFromDir(testdir)
  fs.writeFileSync(testmodule, js.replace(/'tacks'/g, "'../../index.js'"))
  var modelFromModule = require(testmodule)
  return tacksAreTheSame(t, modelFromModule, fixture, 'generateFromDir')
}).catch(test.throws)

test('cleanup', function (t) {
  cleanup()
  try {
    fs.statSync(testdir)
    t.fail(testdir + ' should not exist')
  } catch (ex) {
    t.pass(testdir + ' should not exist')
  }
  rimraf.sync(testroot)
  t.done()
})
