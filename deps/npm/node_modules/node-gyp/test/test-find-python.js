'use strict'

delete process.env.PYTHON

const test = require('tap').test
const findPython = require('../lib/find-python')
const execFile = require('child_process').execFile
const PythonFinder = findPython.test.PythonFinder

require('npmlog').level = 'warn'

test('find python', function (t) {
  t.plan(4)

  findPython.test.findPython(null, function (err, found) {
    t.strictEqual(err, null)
    var proc = execFile(found, ['-V'], function (err, stdout, stderr) {
      t.strictEqual(err, null)
      if (/Python 2/.test(stderr)) {
        t.strictEqual(stdout, '')
        t.ok(/Python 2/.test(stderr))
      } else {
        t.ok(/Python 3/.test(stdout))
        t.strictEqual(stderr, '')
      }
    })
    proc.stdout.setEncoding('utf-8')
    proc.stderr.setEncoding('utf-8')
  })
})

function poison (object, property) {
  function fail () {
    console.error(Error(`Property ${property} should not have been accessed.`))
    process.abort()
  }
  var descriptor = {
    configurable: false,
    enumerable: false,
    get: fail,
    set: fail
  }
  Object.defineProperty(object, property, descriptor)
}

function TestPythonFinder () {
  PythonFinder.apply(this, arguments)
}
TestPythonFinder.prototype = Object.create(PythonFinder.prototype)
// Silence npmlog - remove for debugging
TestPythonFinder.prototype.log = {
  silly: () => {},
  verbose: () => {},
  info: () => {},
  warn: () => {},
  error: () => {}
}
delete TestPythonFinder.prototype.env.NODE_GYP_FORCE_PYTHON

test('find python - python', function (t) {
  t.plan(6)

  var f = new TestPythonFinder('python', done)
  f.execFile = function (program, args, opts, cb) {
    f.execFile = function (program, args, opts, cb) {
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

  function done (err, python) {
    t.strictEqual(err, null)
    t.strictEqual(python, '/path/python')
  }
})

test('find python - python too old', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.execFile = function (program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length - 1])) {
      cb(null, '/path/python')
    } else if (/sys\.version_info/.test(args[args.length - 1])) {
      cb(null, '2.3.4')
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done (err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not supported/i.test(f.errorLog))
  }
})

test('find python - no python', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.execFile = function (program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length - 1])) {
      cb(new Error('not found'))
    } else if (/sys\.version_info/.test(args[args.length - 1])) {
      cb(new Error('not a Python executable'))
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done (err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not in PATH/.test(f.errorLog))
  }
})

test('find python - no python2, no python, unix', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.checkPyLauncher = t.fail
  f.win = false

  f.execFile = function (program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length - 1])) {
      cb(new Error('not found'))
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done (err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not in PATH/.test(f.errorLog))
  }
})

test('find python - no python, use python launcher', function (t) {
  t.plan(3)

  var f = new TestPythonFinder(null, done)
  f.win = true

  f.execFile = function (program, args, opts, cb) {
    if (program === 'py.exe') {
      t.notEqual(args.indexOf('-c'), -1)
      return cb(null, 'Z:\\snake.exe')
    }
    if (/sys\.executable/.test(args[args.length - 1])) {
      cb(new Error('not found'))
    } else if (f.winDefaultLocations.includes(program)) {
      cb(new Error('not found'))
    } else if (/sys\.version_info/.test(args[args.length - 1])) {
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

  function done (err, python) {
    t.strictEqual(err, null)
    t.strictEqual(python, 'Z:\\snake.exe')
  }
})

test('find python - no python, no python launcher, good guess', function (t) {
  t.plan(2)

  var re = /C:[\\/]Python37[\\/]python[.]exe/
  var f = new TestPythonFinder(null, done)
  f.win = true

  f.execFile = function (program, args, opts, cb) {
    if (program === 'py.exe') {
      return cb(new Error('not found'))
    }
    if (/sys\.executable/.test(args[args.length - 1])) {
      cb(new Error('not found'))
    } else if (re.test(program) &&
               /sys\.version_info/.test(args[args.length - 1])) {
      cb(null, '3.7.3')
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done (err, python) {
    t.strictEqual(err, null)
    t.ok(re.test(python))
  }
})

test('find python - no python, no python launcher, bad guess', function (t) {
  t.plan(2)

  var f = new TestPythonFinder(null, done)
  f.win = true

  f.execFile = function (program, args, opts, cb) {
    if (/sys\.executable/.test(args[args.length - 1])) {
      cb(new Error('not found'))
    } else if (/sys\.version_info/.test(args[args.length - 1])) {
      cb(new Error('not a Python executable'))
    } else {
      t.fail()
    }
  }
  f.findPython()

  function done (err) {
    t.ok(/Could not find any Python/.test(err))
    t.ok(/not in PATH/.test(f.errorLog))
  }
})
