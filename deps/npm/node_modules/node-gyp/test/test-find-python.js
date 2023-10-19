'use strict'

delete process.env.PYTHON

const { describe, it } = require('mocha')
const assert = require('assert')
const findPython = require('../lib/find-python')
const execFile = require('child_process').execFile
const PythonFinder = findPython.test.PythonFinder

require('npmlog').level = 'warn'

describe('find-python', function () {
  it('find python', function () {
    findPython.test.findPython(null, function (err, found) {
      assert.strictEqual(err, null)
      var proc = execFile(found, ['-V'], function (err, stdout, stderr) {
        assert.strictEqual(err, null)
        assert.ok(/Python 3/.test(stdout))
        assert.strictEqual(stderr, '')
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

  it('find python - python', function () {
    var f = new TestPythonFinder('python', done)
    f.execFile = function (program, args, opts, cb) {
      f.execFile = function (program, args, opts, cb) {
        poison(f, 'execFile')
        assert.strictEqual(program, '/path/python')
        assert.ok(/sys\.version_info/.test(args[1]))
        cb(null, '3.9.1')
      }
      assert.strictEqual(program,
        process.platform === 'win32' ? '"python"' : 'python')
      assert.ok(/sys\.executable/.test(args[1]))
      cb(null, '/path/python')
    }
    f.findPython()

    function done (err, python) {
      assert.strictEqual(err, null)
      assert.strictEqual(python, '/path/python')
    }
  })

  it('find python - python too old', function () {
    var f = new TestPythonFinder(null, done)
    f.execFile = function (program, args, opts, cb) {
      if (/sys\.executable/.test(args[args.length - 1])) {
        cb(null, '/path/python')
      } else if (/sys\.version_info/.test(args[args.length - 1])) {
        cb(null, '2.3.4')
      } else {
        assert.fail()
      }
    }
    f.findPython()

    function done (err) {
      assert.ok(/Could not find any Python/.test(err))
      assert.ok(/not supported/i.test(f.errorLog))
    }
  })

  it('find python - no python', function () {
    var f = new TestPythonFinder(null, done)
    f.execFile = function (program, args, opts, cb) {
      if (/sys\.executable/.test(args[args.length - 1])) {
        cb(new Error('not found'))
      } else if (/sys\.version_info/.test(args[args.length - 1])) {
        cb(new Error('not a Python executable'))
      } else {
        assert.fail()
      }
    }
    f.findPython()

    function done (err) {
      assert.ok(/Could not find any Python/.test(err))
      assert.ok(/not in PATH/.test(f.errorLog))
    }
  })

  it('find python - no python2, no python, unix', function () {
    var f = new TestPythonFinder(null, done)
    f.checkPyLauncher = assert.fail
    f.win = false

    f.execFile = function (program, args, opts, cb) {
      if (/sys\.executable/.test(args[args.length - 1])) {
        cb(new Error('not found'))
      } else {
        assert.fail()
      }
    }
    f.findPython()

    function done (err) {
      assert.ok(/Could not find any Python/.test(err))
      assert.ok(/not in PATH/.test(f.errorLog))
    }
  })

  it('find python - no python, use python launcher', function () {
    var f = new TestPythonFinder(null, done)
    f.win = true

    f.execFile = function (program, args, opts, cb) {
      if (program === 'py.exe') {
        assert.notStrictEqual(args.indexOf('-3'), -1)
        assert.notStrictEqual(args.indexOf('-c'), -1)
        return cb(null, 'Z:\\snake.exe')
      }
      if (/sys\.executable/.test(args[args.length - 1])) {
        cb(new Error('not found'))
      } else if (f.winDefaultLocations.includes(program)) {
        cb(new Error('not found'))
      } else if (/sys\.version_info/.test(args[args.length - 1])) {
        if (program === 'Z:\\snake.exe') {
          cb(null, '3.9.0')
        } else {
          assert.fail()
        }
      } else {
        assert.fail()
      }
    }
    f.findPython()

    function done (err, python) {
      assert.strictEqual(err, null)
      assert.strictEqual(python, 'Z:\\snake.exe')
    }
  })

  it('find python - no python, no python launcher, good guess', function () {
    var f = new TestPythonFinder(null, done)
    f.win = true
    const expectedProgram = f.winDefaultLocations[0]

    f.execFile = function (program, args, opts, cb) {
      if (program === 'py.exe') {
        return cb(new Error('not found'))
      }
      if (/sys\.executable/.test(args[args.length - 1])) {
        cb(new Error('not found'))
      } else if (program === expectedProgram &&
                 /sys\.version_info/.test(args[args.length - 1])) {
        cb(null, '3.7.3')
      } else {
        assert.fail()
      }
    }
    f.findPython()

    function done (err, python) {
      assert.strictEqual(err, null)
      assert.ok(python === expectedProgram)
    }
  })

  it('find python - no python, no python launcher, bad guess', function () {
    var f = new TestPythonFinder(null, done)
    f.win = true

    f.execFile = function (program, args, opts, cb) {
      if (/sys\.executable/.test(args[args.length - 1])) {
        cb(new Error('not found'))
      } else if (/sys\.version_info/.test(args[args.length - 1])) {
        cb(new Error('not a Python executable'))
      } else {
        assert.fail()
      }
    }
    f.findPython()

    function done (err) {
      assert.ok(/Could not find any Python/.test(err))
      assert.ok(/not in PATH/.test(f.errorLog))
    }
  })
})
