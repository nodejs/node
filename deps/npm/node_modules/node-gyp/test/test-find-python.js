'use strict'

var test = require('tap').test
var configure = require('../lib/configure')
var execFile = require('child_process').execFile
var PythonFinder = configure.test.PythonFinder

require('npmlog').level = 'warn'

test('find python', function (t) {
  t.plan(4)

  configure.test.findPython(null, function (err, found) {
    t.strictEqual(err, null)
    var proc = execFile(found, ['-V'], function (err, stdout, stderr) {
      t.strictEqual(err, null)
      t.strictEqual(stdout, '')
      t.ok(/Python 2/.test(stderr))
    })
    proc.stdout.setEncoding('utf-8')
    proc.stderr.setEncoding('utf-8')
  })
})

function poison(object, property) {
  function fail() {
    console.error(Error(`Property ${property} should not have been accessed.`))
    process.abort()
  }
  var descriptor = {
    configurable: false,
    enumerable: false,
    get: fail,
    set: fail,
  }
  Object.defineProperty(object, property, descriptor)
}

function TestPythonFinder() { PythonFinder.apply(this, arguments) }
TestPythonFinder.prototype = Object.create(PythonFinder.prototype)
// Silence npmlog - remove for debugging
TestPythonFinder.prototype.log = {
  silly: () => {},
  verbose: () => {},
  info: () => {},
  warn: () => {},
  error: () => {},
}

test('find python - python', function (t) {
  t.plan(6)

  var f = new TestPythonFinder('python', done)
  f.execFile = function(program, args, opts, cb) {
    f.execFile = function(program, args, opts, cb) {
      poison(f, 'execFile')
      t.strictEqual(program, '/path/python')
      t.ok(/sys\.version_info/.test(args[1]))
      cb(null, '2.7.15')
    }
    t.strictEqual(program,
                  process.platform === 'win32' ? '"python"' : 'python')
    t.ok(/sys\.executable/.test(args[1]))
    cb(null, '/path/python')
  }
  f.findPython()

  function done(err, python) {
    t.strictEqual(err, null)
    t.strictEqual(python, '/path/python')
  }
})

test('find python - python too old', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.execFile = function(program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(null, '/path/python')
    } else if (/sys\.version_info/.test(args[args.length-1])) {
      cb(null, '2.3.4')
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not supported/i.test(f.errorLog))
  }
})

test('find python - python too new', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.execFile = function(program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(null, '/path/python')
    } else if (/sys\.version_info/.test(args[args.length-1])) {
      cb(null, '3.0.0')
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not supported/i.test(f.errorLog))
  }
})

test('find python - no python', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.execFile = function(program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(new Error('not found'))
    } else if (/sys\.version_info/.test(args[args.length-1])) {
      cb(new Error('not a Python executable'))
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not in PATH/.test(f.errorLog))
  }
})

test('find python - no python2', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.execFile = function(program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length-1])) {
      if (program == 'python2') {
        cb(new Error('not found'))
      } else {
        cb(null, '/path/python')
      }
    } else if (/sys\.version_info/.test(args[args.length-1])) {
      cb(null, '2.7.14')
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err, python) {
    t.strictEqual(err, null)
    t.strictEqual(python, '/path/python')
  }
})

test('find python - no python2, no python, unix', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.checkPyLauncher = t.fail
  f.win = false

  f.execFile = function(program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(new Error('not found'))
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not in PATH/.test(f.errorLog))
  }
})

test('find python - no python, use python launcher', function (t) {
  t.plan(4)

  var f = new TestPythonFinder(null, done)
  f.win = true

  f.execFile = function(program, args, opts, cb) {
    if (program === 'py.exe') {
      t.notEqual(args.indexOf('-2'), -1)
      t.notEqual(args.indexOf('-c'), -1)
      return cb(null, 'Z:\\snake.exe')
    }
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(new Error('not found'))
    } else if (/sys\.version_info/.test(args[args.length-1])) {
      if (program === 'Z:\\snake.exe') {
        cb(null, '2.7.14')
      } else {
        t.fail()
      }
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err, python) {
    t.strictEqual(err, null)
    t.strictEqual(python, 'Z:\\snake.exe')
  }
})

test('find python - python 3, use python launcher', function (t) {
  t.plan(4)

  var f = new TestPythonFinder(null, done)
  f.win = true

  f.execFile = function(program, args, opts, cb) {
    if (program === 'py.exe') {
      f.execFile = function(program, args, opts, cb) {
        poison(f, 'execFile')
        if (/sys\.version_info/.test(args[args.length-1])) {
          cb(null, '2.7.14')
        } else {
          t.fail()
        }
      }
      t.notEqual(args.indexOf('-2'), -1)
      t.notEqual(args.indexOf('-c'), -1)
      return cb(null, 'Z:\\snake.exe')
    }
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(null, '/path/python')
    } else if (/sys\.version_info/.test(args[args.length-1])) {
      cb(null, '3.0.0')
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err, python) {
    t.strictEqual(err, null)
    t.strictEqual(python, 'Z:\\snake.exe')
  }
})

test('find python - python 3, use python launcher, python 2 too old',
     function (t) {
  t.plan(6)

  var f = new TestPythonFinder(null, done)
  f.win = true

  f.execFile = function(program, args, opts, cb) {
    if (program === 'py.exe') {
      f.execFile = function(program, args, opts, cb) {
        if (/sys\.version_info/.test(args[args.length-1])) {
          f.execFile = function(program, args, opts, cb) {
            if (/sys\.version_info/.test(args[args.length-1])) {
              poison(f, 'execFile')
              t.strictEqual(program, f.defaultLocation)
              cb(new Error('not found'))
            } else {
              t.fail()
            }
          }
          t.strictEqual(program, 'Z:\\snake.exe')
          cb(null, '2.3.4')
        } else {
          t.fail()
        }
      }
      t.notEqual(args.indexOf('-2'), -1)
      t.notEqual(args.indexOf('-c'), -1)
      return cb(null, 'Z:\\snake.exe')
    }
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(null, '/path/python')
    } else if (/sys\.version_info/.test(args[args.length-1])) {
      cb(null, '3.0.0')
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not supported/i.test(f.errorLog))
  }
})

test('find python - no python, no python launcher, good guess', function (t) {
  t.plan(4)

  var re = /C:[\\\/]Python27[\\\/]python[.]exe/
  var f = new TestPythonFinder(null, done)
  f.win = true

  f.execFile = function(program, args, opts, cb) {
    if (program === 'py.exe') {
      f.execFile = function(program, args, opts, cb) {
        poison(f, 'execFile')
        t.ok(re.test(program))
        t.ok(/sys\.version_info/.test(args[args.length-1]))
        cb(null, '2.7.14')
      }
      return cb(new Error('not found'))
    }
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(new Error('not found'))
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err, python) {
    t.strictEqual(err, null)
    t.ok(re.test(python))
  }
})

test('find python - no python, no python launcher, bad guess', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.win = true

  f.execFile = function(program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length-1])) {
      cb(new Error('not found'))
    } else if (/sys\.version_info/.test(args[args.length-1])) {
      cb(new Error('not a Python executable'))
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done(err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not in PATH/.test(f.errorLog))
  }
})
