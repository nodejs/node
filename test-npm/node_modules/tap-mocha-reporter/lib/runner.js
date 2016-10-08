// A facade from the tap-parser to the Mocha "Runner" object.
// Note that pass/fail/suite events need to also mock the "Runnable"
// objects (either "Suite" or "Test") since these have functions
// which are called by the formatters.

module.exports = Runner

// relevant events:
//
// start()
//   Start of the top-level test set
//
// end()
//   End of the top-level test set.
//
// fail(test, err)
//   any "not ok" test that is not the trailing test for a suite
//   of >0 test points.
//
// pass(test)
//   any "ok" test point that is not the trailing test for a suite
//   of >0 tests
//
// pending(test)
//   Any "todo" test
//
// suite(suite)
//   A suite is a child test with >0 test points.  This is a little bit
//   tricky, because TAP will provide a "child" event before we know
//   that it's a "suite".  We see the "# Subtest: name" comment as the
//   first thing in the subtest.  Then, when we get our first test point,
//   we know that it's a suite, and can emit the event with the mock suite.
//
// suite end(suite)
//   Emitted when we end the subtest
//
// test(test)
//   Any test point which is not the trailing test for a suite.
//
// test end(test)
//   Emitted immediately after the "test" event because test points are
//   not async in TAP.

var util = require('util')
var Test = require('./test.js')
var Suite = require('./suite.js')
var Writable = require('stream').Writable
if (!Writable) {
  try {
    Writable = require('readable-stream').Writable
  } catch (er) {
    throw new Error('Please install "readable-stream" to use this module ' +
                    'with Node.js v0.8 and before')
  }
}

var Parser = require('tap-parser')

// $1 = number, $2 = units
var timere = /^#\s*time=((?:0|[1-9][0-9]*?)(?:\.[0-9]+)?)(ms|s)?$/

util.inherits(Runner, Writable)

function Runner (options) {
  if (!(this instanceof Runner))
    return new Runner(options)

  var parser = this.parser = new Parser(options)
  this.startTime = new Date()

  attachEvents(this, parser, 0)
  Writable.call(this, options)
}

Runner.prototype.write = function () {
  if (!this.emittedStart) {
    this.emittedStart = true
    this.emit('start')
  }

  return this.parser.write.apply(this.parser, arguments)
}

Runner.prototype.end = function () {
  return this.parser.end.apply(this.parser, arguments)
}

Parser.prototype.fullTitle = function () {
  if (!this.parent)
    return this.name || ''
  else
    return (this.parent.fullTitle() + ' ' + (this.name || '')).trim()
}

function attachEvents (runner, parser, level) {
  parser.runner = runner

  if (level === 0) {
    parser.on('line', function (c) {
      runner.emit('line', c)
    })
    parser.on('version', function (v) {
      runner.emit('version', v)
    })
    parser.on('complete', function (res) {
      runner.emit('end')
    })
    parser.on('comment', function (c) {
      var tmatch = c.trim().match(timere)
      if (tmatch) {
        var t = +tmatch[1]
        if (tmatch[2] === 's')
          t *= 1000
        parser.time = t
        if (runner.stats)
          runner.stats.duration = t
      }
    })
  }

  parser.emittedSuite = false
  parser.didAssert = false
  parser.name = parser.name || ''
  parser.doingChild = null

  parser.on('complete', function (res) {
    if (!res.ok) {
      var fail = { ok: false, diag: {} }
      var count = res.count
      if (res.plan) {
        var plan = res.plan.end - res.plan.start + 1
        if (count !== plan) {
          fail.name = 'test count !== plan'
          fail.diag = {
            found: count,
            wanted: plan
          }
        } else {
          // probably handled on child parser
          return
        }
      } else {
        fail.name = 'missing plan'
      }
      fail.diag.results = res
      emitTest(parser, fail)
    }
  })

  parser.on('child', function (child) {
    child.parent = parser
    attachEvents(runner, child, level + 1)

    // if we're in a suite, but we haven't emitted it yet, then we
    // know that an assert will follow this child, even if there are
    // no others. That means that we will definitely have a 'suite'
    // event to emit.
    emitSuite(this)

    this.didAssert = true
    this.doingChild = child
  })

  if (!parser.name) {
    parser.on('comment', function (c) {
      if (!this.name && c.match(/^# Subtest: /)) {
        c = c.trim().replace(/^# Subtest: /, '')
        this.name = c
      }
    })
  }

  // Just dump all non-parsing stuff to stderr
  parser.on('extra', function (c) {
    process.stderr.write(c)
  })

  parser.on('assert', function (result) {
    emitSuite(this)

    // no need to print the trailing assert for subtests
    // we've already emitted a 'suite end' event for this.
    // UNLESS, there were no other asserts, AND it's root level
    if (this.doingChild) {
      var suite = this.doingChild.suite
      if (this.doingChild.name === result.name) {
        if (suite) {
          if (result.time)
            suite.duration = result.time

          // If it's ok so far, but the ending result is not-ok, then
          // that means that it exited non-zero.  Emit the test so
          // that we can print it as a failure.
          if (suite.ok && !result.ok)
            emitTest(this, result)
        }
      }

      var emitOn = this
      var dc = this.doingChild
      this.doingChild = null

      if (!dc.didAssert && dc.level === 1) {
        emitOn = dc
      } else if (dc.didAssert) {
        if (dc.suite)
          runner.emit('suite end', dc.suite)
        return
      } else {
        emitOn = this
      }

      emitSuite(emitOn)
      emitTest(emitOn, result)
      if (emitOn !== this && emitOn.suite) {
        runner.emit('suite end', emitOn.suite)
        delete emitOn.suite
      }
      if (dc.suite) {
        runner.emit('suite end', dc.suite)
      }
      return
    }

    this.didAssert = true
    this.doingChild = null

    emitTest(this, result)
  })

  parser.on('complete', function (results) {
    this.results = results
  })

  parser.on('bailout', function (reason) {
    var suite = this.suite
    runner.emit('bailout', reason, suite)
    if (suite)
      this.suite = suite.parent
  })

  // proxy all stream events directly
  var streamEvents = [
    'pipe', 'prefinish', 'finish', 'unpipe', 'close'
  ]

  streamEvents.forEach(function (ev) {
    parser.on(ev, function () {
      var args = [ev]
      args.push.apply(args, arguments)
      runner.emit.apply(runner, args)
    })
  })
}

function emitSuite (parser) {
  if (!parser.emittedSuite && parser.name) {
    parser.emittedSuite = true
    var suite = parser.suite = new Suite(parser)
    if (parser.parent && parser.parent.suite)
      parser.parent.suite.suites.push(suite)
    if (parser.runner.stats)
      parser.runner.stats.suites ++

    parser.runner.emit('suite', suite)
  }
}

function emitTest (parser, result) {
  var runner = parser.runner
  var test = new Test(result, parser)

  if (parser.suite) {
    parser.suite.tests.push(test)
    if (!result.ok) {
      for (var p = parser; p && p.suite; p = p.parent) {
        p.suite.ok = false
      }
    }
    parser.suite.ok = parser.suite.ok && result.ok
  }

  runner.emit('test', test)
  if (result.skip || result.todo) {
    runner.emit('pending', test)
  } else if (result.ok) {
    runner.emit('pass', test)
  } else {
    var error = getError(result)
    runner.emit('fail', test, error)
  }
  runner.emit('test end', test)
}

function getError (result) {
  var err

  function reviveStack (stack) {
    if (!stack)
      return null

    return stack.trim().split('\n').map(function (line) {
      return '    at ' + line
    }).join('\n')
  }

  if (result.diag && result.diag.error) {
    err = {
      name: result.diag.error.name || 'Error',
      message: result.diag.error.message,
      toString: function () {
        return this.name + ': ' + this.message
      },
      stack: result.diag.error.stack
    }
  } else {
    err = {
      message: (result.name || '(unnamed error)').replace(/^Error: /, ''),
      toString: function () {
        return 'Error: ' + this.message
      },
      stack: result.diag && result.diag.stack
    }
  }

  var diag = result.diag

  if (err.stack)
    err.stack = err.toString() + '\n' + reviveStack(err.stack)

  if (diag) {
    var hasFound = diag.hasOwnProperty('found')
    var hasWanted = diag.hasOwnProperty('wanted')

    if (hasFound)
      err.actual = diag.found

    if (hasWanted)
      err.expected = diag.wanted

    if (hasFound && hasWanted)
      err.showDiff = true
  }

  return err
}

