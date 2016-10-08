module.exports = Test

var Readable = require('stream').Readable
/* istanbul ignore if */
if (!Readable || process.version.match(/^v0\.10/)) {
  Readable = require('readable-stream').Readable
}

var Promise = require('bluebird')

function Deferred () {
  this.resolve = null
  this.reject = null
  this.promise = new Promise(function (resolve, reject) {
    this.reject = reject
    this.resolve = resolve
  }.bind(this))
}

var util = require('util')
util.inherits(Test, Readable)

// A sigil object for implicit end() calls that should not
// trigger an error if the user then calls t.end()
var IMPLICIT = {}

var yaml = require('js-yaml')
var stack = require('./stack.js')
var tapAsserts = require('./assert.js')
var assert = require('assert')
var spawn = require('child_process').spawn
var Parser = require('tap-parser')
var path = require('path')
var Module = require('module').Module
var fs = require('fs')
var cleanYamlObject = require('clean-yaml-object')
var binpath = path.resolve(__dirname, '../bin')

function hasOwn (obj, key) {
  return Object.prototype.hasOwnProperty.call(obj, key)
}

function Test (options) {
  options = options || {}

  if (!(this instanceof Test)) {
    return new Test(options)
  }

  this.assertStack = null
  this.assertAt = null

  this._lineBuf = ''
  this._deferred = null
  this._autoend = !!options.autoend
  this._name = options.name || '(unnamed test)'
  this._ok = true
  this._pass = 0
  this._fail = 0
  this._skip = 0
  this._todo = 0
  this._count = 0
  this._bailedOut = false
  this._endEmitted = false
  this._explicitEnded = false
  this._multiEndThrew = false
  this._ranAfterEach = false

  if (Object.prototype.hasOwnProperty.call(options, 'bail')) {
    this._bail = !!options.bail
  } else {
    this._bail = process.env.TAP_BAIL === '1'
  }

  this._passes = []
  this._fails = []
  this._skips = []
  this._todos = []

  this._beforeEach = []
  this._afterEach = []

  this._plan = -1
  this._queue = []
  this._currentChild = null
  this._ending = false
  this._ended = false
  this._mustDeferEnd = false
  this._deferredEnd = null

  this._parent = null
  this._printedVersion = false

  this._startTime = process.hrtime()
  this._calledAt = options.at || stack.at(this.test)
  if (!this._calledAt || !this._calledAt.file) {
    this._calledAt = stack.at(Test)
  }

  this._timer = null
  this._timeout = 0
  if (options.timeout !== Infinity &&
    !isNaN(options.timeout) &&
    options.timeout > 0) {
    this.setTimeout(options.timeout)
  }

  Readable.apply(this, options)

  // Bind all methods.
  var bound = {}
  for (var m in this) {
    if (typeof this[m] === 'function') {
      this[m] = this[m].bind(this)
      bound[m] = true
    }
  }
  Object.getOwnPropertyNames(Test.prototype).forEach(function (name) {
    if (typeof this[name] === 'function' && !bound[name]) {
      Object.defineProperty(this, name, {
        value: this[name].bind(this),
        enumerable: false,
        configurable: true,
        writable: true
      })
    }
  }, this)
}

Test.prototype.tearDown = Test.prototype.teardown = function (fn) {
  this.on('end', fn)
}

Test.prototype.setTimeout = function (n) {
  if (n === Infinity) {
    if (this._timer) {
      clearTimeout(this._timer)
      this._timer = null
    }
    this._timeout = 0
    return
  }

  if (isNaN(n) || n <= 0) {
    throw new TypeError('setTimeout: number > 0 required')
  }

  this._timeout = n
  if (this._timer) {
    clearTimeout(this._timer)
  }

  var self = this
  this._timer = setTimeout(function () {
    self._onTimeout()
  }, n)
}

Test.prototype._onTimeout = function (extra) {
  clearTimeout(this._timer)
  this._timer = null
  // anything that was pending will have to wait.
  var s = this
  while (s._currentChild && (s._currentChild instanceof Test)) {
    s._queue = []
    s.end(IMPLICIT)
    s = s._currentChild
  }

  extra = extra || {}
  if (this._parent) {
    extra.expired = this._name
  }
  if (this._timeout) {
    extra.timeout = this._timeout
  }
  extra.at = this._calledAt
  s.fail('timeout!', extra)
  s.end(IMPLICIT)

  this.endAll()
}

// Called when endAll() is fired and there's stuff in the queue
Test.prototype._queueFail = function () {
  var queue = this._queue
  this._queue = []

  queue.forEach(function (q) {
    var what = q[0]
    var msg = what + ' left in queue'
    var extra = { at: this._calledAt }

    switch (what) {
      case 'test':
        extra = q[2]
        msg = 'child test left in queue: ' + (q[1] || '(unnamed)')
        break

      case 'printResult':
        if (q[2] === 'test unfinished: ' + (this._name || '(unnamed)')) {
          return
        }
        msg = (q[1] ? 'ok' : 'not ok') + ' - ' + q[2].trim()
        msg = 'test point left in queue: ' + msg
        extra = q[3]
        extra.at = extra.at || null
        break

      case 'spawn':
        extra = q[5]
        extra.command = q[1]
        extra.args = q[2]
        extra.options = q[3]
        msg = 'spawn left in queue: ' + (q[4] || '(unnamed)')
        break

      case 'end':
        return
    }

    if (this._parent) {
      extra.test = this._name
    }

    this.fail(msg, extra)
  }, this)
}

Test.prototype.endAll = function () {
  var child = this._currentChild

  if (this._queue && this._queue.length) {
    this._queueFail()
  }

  if (child) {
    if (!child._ended && child.fail) {
      var msg = 'test unfinished: ' + (child._name || '(unnamed)')
      var extra = { at: child._calledAt }
      if (child._plan !== -1) {
        extra.plan = child._plan
        extra.count = child._count
      }
      child.fail(msg, extra)
    }

    if (child.end) {
      child.end(IMPLICIT)
    }

    if (child.endAll) {
      child.endAll()
    }

    if (child.kill) {
      child.kill('SIGTERM')
    }

    child._bailedOut = true
  }

  this.end(IMPLICIT)
}

Test.prototype._extraFromError = function (er, extra) {
  extra = extra || {}
  if (!er || typeof er !== 'object') {
    extra.error = er
    return extra
  }

  var message = er.message
  var addName = true

  if (!message && er.stack) {
    message = er.stack.split('\n')[0]
    addName = false
  }

  er.message = ''
  var st = er.stack
  if (st) {
    st = st.split('\n')
    // parse out the 'at' bit from the first line.
    extra.at = stack.parseLine(st[1])
    extra.stack = stack.clean(st)
  }
  er.message = message

  if (addName && er.name) {
    message = er.name + ': ' + message
  }

  Object.keys(er).forEach(function (k) {
    if (k === 'message') {
      return
    }
    extra[k] = er[k]
  })

  extra.message = message

  return extra
}

Test.prototype.threw = function threw (er, extra, proxy) {
  this._ok = false
  this._threw = this._threw || er

  if (this._name && !proxy) {
    er.test = this._name
  }

  // If we've already ended, then try to pass this up the chain
  // Presumably, eventually the root harness will catch it and
  // deal with it, since that only ends on process exit.
  if (this._ended) {
    if (this._parent) {
      return this._parent.threw(er, extra, true)
    } else {
      throw er
    }
  }

  if (!extra) {
    extra = this._extraFromError(er)
  }

  this.fail(extra.message || er.message, extra)

  // thrown errors cut to the front of the queue.
  if (this._currentChild && this._queue.length) {
    this._queue.unshift(this._queue.pop())
  }
  if (!proxy) {
    this.end(IMPLICIT)
  }
}

Test.prototype.pragma = function (set) {
  if (this._bailedOut) {
    return
  }

  if (this._currentChild) {
    this._queue.push(['pragma', set])
    return
  }

  Object.keys(set).forEach(function (i) {
    this.push('pragma ' + (set[i] ? '+' : '-') + i + '\n')
  }, this)
}

Test.prototype.plan = function (n, comment) {
  if (this._bailedOut) {
    return
  }

  if (this._currentChild) {
    this._queue.push(['plan', n, comment])
    return
  }

  if (this._plan !== -1) {
    throw new Error('Cannot set plan more than once')
  }

  if (typeof n !== 'number' || n < 0) {
    throw new TypeError('plan must be a number')
  }

  // Cannot get any tests after a trailing plan, or a plan of 0
  var ending = false
  if (this._count !== 0 || n === 0) {
    ending = true
  }

  this._plan = n
  comment = comment ? ' # ' + comment.trim() : ''
  this.push('1..' + n + comment + '\n')

  if (ending) {
    this.end(IMPLICIT)
  }
}

Test.prototype._parseTestArgs = function (name, extra, cb) {
  if (typeof name === 'function') {
    cb = name
    name = ''
    extra = {}
  } else if (typeof name === 'object') {
    cb = extra
    extra = name
    name = ''
  } else if (typeof extra === 'function') {
    cb = extra
    extra = {}
  }

  if (!extra) {
    extra = {}
  }

  if (!cb) {
    extra.todo = true
  } else if (typeof cb !== 'function') {
    throw new TypeError('test() callback must be function if provided')
  }

  if (!name && cb && cb.name) {
    name = cb.name
  }

  name = name || '(unnamed test)'
  return [name, extra, cb]
}

Test.prototype.test = function test (name, extra, cb, deferred) {
  if (!deferred) {
    deferred = new Deferred()
  }

  if (this._bailedOut) {
    deferred.resolve(this)
    return deferred.promise
  }

  if (this._autoendTimer) {
    clearTimeout(this._autoendTimer)
  }

  var args = this._parseTestArgs(name, extra, cb)
  name = args[0]
  extra = args[1]
  cb = args[2]

  if (extra.skip || extra.todo) {
    this.pass(name, extra)
    deferred.resolve(this)
    return deferred.promise
  }

  // will want this captured now in case child fails.
  if (!hasOwn(extra, 'at')) {
    extra.at = stack.at(test)
  }

  if (this._currentChild) {
    this._queue.push(['test', name, extra, cb, deferred])
    return deferred.promise
  }

  var child = new Test(extra)
  this.comment('Subtest: ' + name)

  child._name = name
  child._parent = this
  if (!Object.prototype.hasOwnProperty.call(extra, 'bail')) {
    child._bail = this._bail
  }

  this._currentChild = child

  var self = this
  childStream(self, child)
  self._level = child
  child._deferred = deferred
  this._runBeforeEach(child, function () {
    self._runChild(child, name, extra, cb)
  })
  return deferred.promise
}

Test.prototype._runBeforeEach = function (who, cb) {
  var self = this
  if (this._parent) {
    this._parent._runBeforeEach(who, function () {
      loop(who, self._beforeEach, cb)
    })
  } else {
    loop(who, self._beforeEach, cb)
  }
}

Test.prototype._runAfterEach = function (who, cb) {
  var self = this
  loop(who, self._afterEach, function () {
    if (self._parent) {
      self._parent._runAfterEach(who, cb)
    } else {
      cb()
    }
  })
}

function loop (self, arr, cb, i) {
  if (!i) {
    i = 0
  }

  // this weird little engine is to loop if the cb's keep getting
  // called synchronously, since that's faster and makes shallower
  // stack traces, but recurse if any of them don't fire this tick
  var running = false
  while (i < arr.length && !running) {
    running = true
    var sync = true
    var ret = arr[i].call(self, next)
    if (ret && typeof ret.then === 'function') {
      ret.then(next, self.threw)
    }
    sync = false
    i++
  }

  function next (er) {
    if (er) {
      return self.threw(er)
    } else if (!sync) {
      return loop(self, arr, cb, i + 1)
    }
    running = false
  }

  if (i >= arr.length && !running) {
    return cb()
  }
}

Test.prototype._runChild = function runChild (child, name, extra, cb) {
  var self = this
  var results
  child.on('complete', function (res) {
    results = pruneFailures(res)
  })
  child.on('end', function () {
    extra.results = results
    self._currentChild = null
    if (results) {
      name += ' # time=' + results.time + 'ms'
    }
    self.ok(child._ok, name, extra)
    if (!self._ended) {
      self.push('\n')
    }
  })
  child.on('bailout', function (message) {
    rootBail(self, message)
  })

  // still need try/catch for synchronous errors
  try {
    child._mustDeferEnd = true
    var cbRet = cb(child)
    if (cbRet && typeof cbRet.then === 'function') {
      // promise
      cbRet.then(function () {
        child._mustDeferEnd = false
        if (!child._tryResumeEnd()) {
          child.end(IMPLICIT)
        }
      }, function (err) {
        child._mustDeferEnd = false
        // raise the error, even if the test is done
        child.threw(err)
        child._tryResumeEnd()
      })
    } else {
      child._mustDeferEnd = false
      child._tryResumeEnd()
    }
  } catch (er) {
    child._mustDeferEnd = false
    child._tryResumeEnd()
    child.threw(er)
  }
  self._level = self
}

Test.prototype.current = function () {
  var t = this
  while (t._level && t !== t._level) {
    t = t._level
  }
  return t
}

// stdin is a bit different than a typical child stream
// It's not treated as a "child test", because we typically
// don't want to indent it or treat as a suite in reporters.
// This is most often used by the runner when - is passed
// as an arg, to run a reporter on a previous run.
// We DO however need to parse it to set the exit failure.
Test.prototype.stdin = function (name, extra, deferred) {
  if (typeof name === 'object') {
    extra = name
    name = null
  }

  if (!name) {
    name = '/dev/stdin'
  }

  if (!extra) {
    extra = {}
  }

  if (!extra.at) {
    extra.at = stack.at(stdin)
  }

  if (!deferred) {
    deferred = new Deferred()
  }

  if (this._currentChild) {
    this._queue.push(['stdin', name, extra, deferred])
    return deferred.promise
  }

  if (extra.skip) {
    this.pass(name, extra)
    deferred.resolve(this)
    return deferred.promise
  }

  var stdin = process.stdin
  this.comment('Subtest: ' + name)
  this._currentChild = stdin
  var start = process.hrtime()
  var parser = new Parser({ preserveWhitespace: true })
  var self = this

  childStream(self, stdin, parser)

  parser.on('complete', function (res) {
    self._currentChild = null
    extra.results = pruneFailures(res)
    var dur = process.hrtime(start)
    var time = Math.round(dur[0] * 1e6 + dur[1] / 1e3) / 1e3
    name += ' # time=' + time + 'ms'
    self.ok(res.ok, name, extra)
    if (!self._ended) {
      self.push('\n')
    }
    deferred.resolve(self)
    self._processQueue()
  })

  parser.on('bailout', function (message) {
    rootBail(self, message)
  })

  process.stdin.resume()
  return deferred.promise
}

function pruneFailures (res) {
  if (res.failures) {
    res.failures = res.failures.filter(function (f) {
      return f.tapError
    })
    if (!res.failures.length) {
      delete res.failures
    }
  }
  return res
}

function rootBail (self, message) {
  var p = self
  while (p._parent) {
    p._bailedOut = true
    p = p._parent
  }
  p.bailout(message)
}

function childStream (self, child, parser) {
  var bailedOut = false
  var emitter = parser || child

  if (parser) {
    if (self._bail) {
      bailOnFail(self, child, parser)
    }

    // The only thing that's actually *required* to be a valid TAP output
    // is a plan and/or at least one ok/not-ok line.  If we don't get any
    // of those, then emit a bogus 1..0 so empty TAP streams read as a
    // skipped test, instead of error.
    var sawTests = false

    parser.once('plan', function () {
      sawTests = true
    })

    parser.once('assert', function () {
      sawTests = true
    })

    child.on('data', function (c) {
      parser.write(c)
    })

    child.on('end', function () {
      if (!sawTests) {
        parser.write('1..0\n')
      }
      parser.end()
    })
  }

  emitter.on('bailout', function (reason) {
    rootBail(self, reason)
    bailedOut = true
  })

  emitter.on('line', function (line) {
    if (bailedOut) {
      return
    }

    if (line.match(/^TAP version \d+\n$/)) {
      return
    }

    if (line.trim()) {
      line = '    ' + line
    } else {
      line = '\n'
    }

    self.push(line)
  })
}

Test.prototype.spawn = function spawnTest (cmd, args, options, name, extra, deferred) {
  if (typeof args === 'string') {
    args = [ args ]
  }

  args = args || []

  if (typeof options === 'string') {
    name = options
    options = {}
  }

  options = options || {}
  if (!name) {
    if (cmd === process.execPath) {
      name = args.map(function (a) {
        if (a.indexOf(process.cwd()) === 0) {
          return './' + a.substr(process.cwd().length + 1).replace(/\\/g, '/')
        } else {
          return a
        }
      }).join(' ')
    } else {
      name = cmd + ' ' + args.join(' ')
    }
  }

  extra = extra || {}

  assert.equal(typeof cmd, 'string')
  assert(Array.isArray(args))
  assert.equal(options && typeof options, 'object')
  assert.equal(typeof name, 'string')
  assert.equal(extra && typeof extra, 'object')

  // stdout must be a pipe
  if (options.stdio) {
    if (typeof options.stdio === 'string') {
      options.stdio = [ options.stdio, 'pipe', options.stdio ]
    } else {
      options.stdio[1] = 'pipe'
    }
  }

  // will want this captured now in case child fails, before enqueue
  if (!hasOwn(extra, 'at')) {
    extra.at = stack.at(spawnTest)
  }

  if (this._autoendTimer) {
    clearTimeout(this._autoendTimer)
  }

  if (!deferred) {
    deferred = new Deferred()
  }

  if (this._currentChild) {
    this._queue.push(['spawn', cmd, args, options, name, extra, deferred])
    return deferred.promise
  }

  if (extra.skip || extra.todo) {
    this.pass(name, extra)
    deferred.resolve(this)
    return deferred.promise
  }

  if (this._bail || options.bail) {
    if (!options.env) {
      options.env = Object.keys(process.env).reduce(function (env, k) {
        env[k] = process.env[k]
        return env
      }, {})
    }
    options.env.TAP_BAIL = '1'
  }

  var start = process.hrtime()
  try {
    var child = spawn(cmd, args, options)
  } catch (er) {
    this.threw(er)
    deferred.resolve(this)
    return deferred.promise
  }

  child.on('error', function (er) {
    this.threw(er)

    // unhook entirely
    child.stdout.removeAllListeners('data')
    child.stdout.removeAllListeners('end')
    child.removeAllListeners('close')
    this._currentChild = null
    deferred.resolve(this)
    this._processQueue()

    // just to be safe, kill the process
    child.kill('SIGKILL')
  }.bind(this))

  var parser = new Parser({ preserveWhitespace: true })
  var self = this
  this.comment('Subtest: ' + name)
  this._currentChild = child

  childStream(self, child.stdout, parser)

  var results
  parser.on('complete', function (res) {
    results = pruneFailures(res)
  })

  if (extra.timeout) {
    var timer = setTimeout(function () {
      extra.failure = 'timeout'
      child.kill('SIGTERM')
      if (process.platform === 'win32') {
        // Sigterm is not catchable on Windows, so report timeout
        reportTimeout(child.stdout, parser)
      } else {
        // give it 1 more second to catch the SIGTERM and finish up
        timer = setTimeout(function () {
          reportTimeout(child.stdout, parser)
          child.kill('SIGKILL')
        }, 1000)
      }
    }, extra.timeout)
  }

  child.on('close', function onclose (code, signal) {
    clearTimeout(timer)
    self._currentChild = null
    extra.results = results
    var dur = process.hrtime(start)
    var time = Math.round(dur[0] * 1e6 + dur[1] / 1e3) / 1e3
    if (code) {
      extra.exitCode = code
    }
    if (signal) {
      extra.signal = signal
    }
    extra.command = cmd
    extra.arguments = args

    if (signal || code) {
      results.ok = false
    }

    if (results.count === 0 && !signal && !code) {
      extra.skip = 'No tests found'
      if (results.plan && results.plan.skipReason) {
        extra.skip = results.plan.skipReason
      }
    } else {
      name += ' # time=' + time + 'ms'
    }

    self.ok(results.ok && !code && !signal, name, extra)
    if (!self._ended) {
      self.push('\n')
    }
    deferred.resolve(self)
    self._processQueue()
  })

  parser.on('bailout', function (message) {
    child.kill('SIGTERM')
  })

  if (child.stderr) {
    child.stderr.on('data', function (c) {
      process.stderr.write(c)
    })
  }

  return deferred.promise
}

// On Unix systems, a SIGTERM can be caught, and the child can
// thus be trusted to report timeouts appropriately.
//
// However, on Windows, SIGTERM cannot be caught, so the child
// stream won't be able to report what was happening when the
// test timed out.  On Unix systems, a SIGTERM will only be caught and
// reported if it's fatal, so if a test does their own stuff with it,
// we send a SIGKILL a second later.
//
// In both cases, this function prevents "plan not found" errors
// resulting from the output being abruptly cut off.
function reportTimeout (stream, parser, ind) {
  ind = ind || ''
  if (parser.child) {
    reportTimeout(stream, parser.child, ind + '    ')
  }
  var count = (parser.count || 0) + 1
  if (parser.current) {
    count += 1
  }
  var timeoutfail = '\n' + ind + 'not ok ' + count + ' - timeout\n'

  stream.emit('data', timeoutfail)
  if (parser.planEnd === -1) {
    stream.emit('data', '\n' + ind + '1..' + count + ' # timeout\n')
  }
}

function bailOnFail (self, stream, parser) {
  parser.on('child', function (c) {
    bailOnFail(self, stream, c)
  })

  parser.on('assert', function (res) {
    if (!res.todo && !res.skip && !res.ok) {
      var ind = new Array(parser.level * 4 + 1).join(' ')
      parser.buffer = ''
      parser.write(ind + 'Bail out! # ' + res.name + '\n')
    }
  })
}

Test.prototype.done = Test.prototype.end = function end (implicit) {
  if (this._bailedOut) {
    return
  }

  if (this._currentChild) {
    this._queue.push(['end', implicit])
    return
  }

  // Repeated end() calls should be run once any deferred ends have finished.
  if (this._deferredEnd) {
    this._deferredEnd.after.push(function () {
      this.end(implicit)
    })
    return
  }

  if (implicit !== IMPLICIT && !this._multiEndThrew) {
    if (this._explicitEnded) {
      this._multiEndThrew = true
      this.threw(new Error('test end() method called more than once'))
      return
    }
    this._explicitEnded = true
  }

  if (this._ended || this._ending) {
    return
  }

  // If neccessary defer the end until the child callback has returned.
  if (this._mustDeferEnd) {
    this._deferredEnd = {
      implicit: implicit,
      plan: this._plan === -1 ? this._count : this._plan,
      after: []
    }
    return
  }

  this._finishEnd(implicit)
}

Test.prototype._finishEnd = function (implicit) {
  // If the afterEach function throws, then we can end up back here
  if (this._ranAfterEach || !this._parent) {
    this._end(implicit)
    return
  }

  this._ranAfterEach = true
  var self = this
  this._parent._runAfterEach(this, function () {
    self._end(implicit)
  })
}

Test.prototype._maybeTimeout = function () {
  if (this._timer) {
    var dur = process.hrtime(this._startTime)
    var time = Math.round(dur[0] * 1e6 + dur[1] / 1e3) / 1e3
    if (time > this._timeout) {
      this._onTimeout()
    } else {
      clearTimeout(this._timer)
      this._timer = null
    }
  }
}

Test.prototype._end = function (implicit) {
  this._maybeTimeout()

  // Emiting the missing tests can trigger a call to end()
  // guard against that with the 'ending' flag
  this._ending = true
  var missing = this._plan - this._count
  while (missing > 0) {
    this.fail('missing test', { at: false })
    missing--
  }

  if (this._plan === -1) {
    this.plan(this._count)
  }

  var final = {
    plan: { start: 1, end: this._plan },
    count: this._count,
    pass: this._pass,
    ok: this._ok
  }

  if (this._fail) {
    final.fail = this._fail
  }

  if (this._bailedOut) {
    final.bailout = true
  }

  if (this._todo) {
    final.todo = this._todo
  }

  if (this._skip) {
    final.skip = this._skip
  }

  this._ended = true

  // This is nice, but too noisy.
  // TODO: prove-style commenting at the end.
  // this.comment(final)
  if (!this._ok) {
    // comment a bit at the end so we know what happened.
    if (this._plan !== this._count) {
      this.comment('actual test count(%d) != plan(%d)',
        this._count, this._plan)
    }

    if (this._fail > 0) {
      this.comment('failed %d of %d tests', this._fail, this._count)
    }
    if (this._todo > 0) {
      this.comment('todo: %d', this._todo)
    }
    if (this._skip > 0) {
      this.comment('skip: %d', this._skip)
    }
  }

  var dur = process.hrtime(this._startTime)
  final.time = Math.round(dur[0] * 1e6 + dur[1] / 1e3) / 1e3
  if (!this._parent) {
    this.comment('time=%sms', final.time)
  }

  this.emit('complete', final)
  if (!this._endEmitted) {
    if (this._lineBuf) {
      this.emit('line', this._lineBuf + '\n')
      this._lineBuf = ''
    }

    this._endEmitted = true
    this.emit('end')
    if (this._deferred) {
      this._deferred.resolve(this._parent)
    }
    if (this._parent) {
      this._parent._processQueue()
    }
  }
}

Test.prototype._tryResumeEnd = function () {
  if (!this._deferredEnd) {
    return false
  }

  var implicit = this._deferredEnd.implicit
  var after = this._deferredEnd.after
  this._deferredEnd = null

  this._finishEnd(implicit)
  after.forEach(function (fn) {
    fn.call(this)
  }, this)

  return true
}

Test.prototype._processQueue = function () {
  if (this._bailedOut) {
    return
  }

  if (this._processingQueue) {
    return
  }

  this._processingQueue = true
  while (this._queue.length && !this._currentChild) {
    var q = this._queue.shift()
    var m = q.shift()
    this[m].apply(this, q)
  }
  this._processingQueue = false
}

Test.prototype._shouldAutoend = function () {
  var should = (
  this._autoend &&
    !this._currentChild &&
    !this._queue.length &&
    !this._ending &&
    this._plan === -1
  )
  return should
}

Test.prototype._maybeAutoend = function () {
  if (this._autoendTimer) {
    clearTimeout(this._autoendTimer)
  }

  if (this._shouldAutoend()) {
    var self = this
    self._autoendTimer = setTimeout(function () {
      if (self._shouldAutoend()) {
        self._autoendTimer = setTimeout(function () {
          if (self._shouldAutoend()) {
            self.end(IMPLICIT)
            self._bailedOut = true
          }
        })
      }
    })
  }
}

Test.prototype.autoend = function () {
  this._autoend = true
  this._maybeAutoend()
}

Test.prototype._read = function (n, cb) {}

Test.prototype.printResult = function printResult (ok, message, extra) {
  if (this._bailedOut) {
    return
  }

  if (typeof ok !== 'boolean') {
    throw new TypeError('boolean `ok` required')
  }

  if (message === undefined) {
    throw new TypeError('string `message` required')
  }

  message += ''

  if (extra && typeof extra !== 'object') {
    throw new TypeError('optional `extra` arg must be object')
  }

  extra = extra || {}

  var n = this._count + 1
  if (this._plan !== -1 && n > this._plan || this._deferredEnd) {
    // Don't over-report failures.
    // it's already had problems, just ignore this.
    if (!this._ok || this._deferredEnd && this._deferredEnd.after.length > 0) {
      return
    }

    var failMessage
    if (this._explicitEnded) {
      failMessage = 'test after end() was called'
    } else {
      failMessage = 'test count exceeds plan'
    }

    var er = new Error(failMessage)
    Error.captureStackTrace(er, this._currentAssert || printResult)
    var c = this
    var name = this._name
    for (c = c._parent; c && c.parent; c = c.parent) {
      name += ' < ' + (c._name || '(unnamed test)')
    }
    er.test = name
    er.count = n
    // Only throw the failure once the deferred end has finished.
    if (this._deferredEnd) {
      er.plan = this._deferredEnd.plan
      extra = this._extraFromError(er)
      this._deferredEnd.after.push(function () {
        this._parent.threw(er, extra, true)
      })
    } else {
      er.plan = this._plan
      this.threw(er)
    }
    return
  }

  if (this.assertAt) {
    extra.at = this.assertAt
    this.assertAt = null
  }

  if (this.assertStack) {
    extra.stack = this.assertStack
    this.assertStack = null
  }

  if (hasOwn(extra, 'stack') && !hasOwn(extra, 'at')) {
    extra.at = stack.parseLine(extra.stack.split('\n')[0])
  }

  var fn = this._currentAssert
  this._currentAssert = null
  if (!ok && !extra.skip && !hasOwn(extra, 'at')) {
    assert.equal(typeof fn, 'function')
    extra.at = stack.at(fn)
    if (!extra.todo) {
      extra.stack = stack.captureString(80, fn)
    }
  }

  if (this._autoendTimer) {
    clearTimeout(this._autoendTimer)
  }

  if (this._currentChild) {
    // Might get abandoned in queue
    if (!hasOwn(extra, 'at')) {
      extra.at = stack.at(fn)
    }
    this._queue.push(['printResult', ok, message, extra])
    return
  }

  var res = { ok: ok, message: message, extra: extra }

  if (extra.todo) {
    this._todo++
    this._todos.push(res)
  }

  if (extra.skip) {
    this._skip++
    this._skips.push(res)
  }

  if (ok) {
    this._pass++
    this._passes.push(res)
  } else {
    this._fail++
    this._fails.push(res)
  }
  this._count++

  this.emit('result', res)
  if (message) {
    message = ' - ' + message
  }

  message = message.replace(/[\n\r]/g, ' ').replace(/\t/g, '  ')

  this.push(util.format('%s %d%s', ok ? 'ok' : 'not ok', n, message))

  var ending = false
  if (n === this._plan) {
    ending = true
  }

  if (extra.skip) {
    this.push(' # SKIP')
    if (typeof extra.skip === 'string') {
      this.push(' ' + extra.skip)
    }
  } else if (extra.todo) {
    this._todo++
    this.push(' # TODO')
    if (typeof extra.todo === 'string') {
      this.push(' ' + extra.todo)
    }
  }

  if (!ok && !extra.todo && !extra.skip) {
    this._ok = false
  }

  this.push('\n')

  // If we're skipping, no need for diags.
  if (!ok && !extra.skip || extra.diagnostic) {
    this.writeDiags(extra)
  }

  if (this._bail && !ok && !extra.skip && !extra.todo) {
    this.bailout(message.replace(/^ - /, ''))
  }

  if (ending) {
    this.end(IMPLICIT)
  }

  this._maybeAutoend()
}

function yamlFilter (propertyName, isRoot, source, target) {
  if (!isRoot) {
    return true
  }

  if (propertyName === 'stack') {
    target.stack = source.stack
    return false
  }

  return !(propertyName === 'todo' ||
  propertyName === 'skip' ||
  propertyName === 'diagnostic' ||
  (propertyName === 'at' && !source[propertyName]))
}

Test.prototype.writeDiags = function (extra) {
  var file = extra.at && extra.at.file && path.resolve(extra.at.file)
  if (file && (file.indexOf(__dirname) === 0 || file.indexOf(binpath) === 0)) {
    delete extra.at
  }

  if (extra.at && extra.at.file && extra.at.line && !extra.source) {
    var content
    file = path.resolve(extra.at.file)
    try {
      content = Module.wrap(fs.readFileSync(file))
    } catch (er) {}
    if (content) {
      content = (content.split('\n')[extra.at.line - 1] || '').trim()
      if (content) {
        extra.source = content + '\n'
      }
    }
  }

  // some objects are not suitable for yaml.
  var obj = cleanYamlObject(extra, yamlFilter)

  if (obj && typeof obj === 'object' && Object.keys(obj).length) {
    var y = yaml.safeDump(obj).split('\n').map(function (l) {
      return l.trim() ? '  ' + l : l.trim()
    }).join('\n')
    y = '  ---\n' + y + '  ...\n'
    this.push(y)
  }
}

Test.prototype.passing = function () {
  return this._ok
}

Test.prototype.pass = function pass (message, extra) {
  if (!this._currentAssert) {
    this._currentAssert = pass
  }
  this.printResult(true, message || '(unnamed test)', extra)
  return true
}

Test.prototype.fail = function fail (message, extra) {
  if (!this._currentAssert) {
    this._currentAssert = fail
  }

  if (message && typeof message === 'object') {
    extra = message
    message = ''
  } else {
    if (!message) {
      message = ''
    }
    if (!extra) {
      extra = {}
    }
  }

  this.printResult(false, message, extra)

  var ret = true
  if (!extra.todo && !extra.skip) {
    ret = false
    // Don't modify test state when there's a deferred end.
    if (!this._deferredEnd) {
      this._ok = false
    }
  }

  return ret
}

Test.prototype.addAssert = function (name, length, fn) {
  if (!name) {
    throw new TypeError('name is required for addAssert')
  }

  if (!(typeof length === 'number' && length >= 0)) {
    throw new TypeError('number of args required')
  }

  if (typeof fn !== 'function') {
    throw new TypeError('function required for addAssert')
  }

  if (Test.prototype[name] || this[name]) {
    throw new TypeError('attempt to re-define `' + name + '` assert')
  }

  this[name] = function ASSERT () {
    if (!this._currentAssert) {
      this._currentAssert = ASSERT
    }
    var args = new Array(length + 2)
    for (var i = 0; i < length; i++) {
      args[i] = arguments[i]
    }
    if (typeof arguments[length] === 'object') {
      args[length] = ''
      args[length + 1] = arguments[length]
    } else {
      args[length] = arguments[length] || ''
      args[length + 1] = arguments[length + 1] || {}
    }

    return fn.apply(this, args)
  }
}

Test.prototype.comment = function () {
  if (this._bailedOut) {
    return
  }

  var message = util.format.apply(util, arguments)

  if (this._currentChild) {
    this._queue.push(['comment', message])
    return
  }

  message = '# ' + message.split(/\r?\n/).join('\n# ')

  this.push(message + '\n')
}

Test.prototype.push = function (c, e) {
  if (this._bailedOut) {
    return true
  }

  assert.equal(typeof c, 'string')

  // We *always* want data coming out immediately.  Test runners have a
  // very special relationship with zalgo. It's more important to ensure
  // that any console.log() lines that appear in the midst of tests are
  // not taken out of context
  if (this._readableState) {
    this._readableState.sync = false
  }

  if (!this._printedVersion && !this._parent) {
    this._printedVersion = true
    this.emit('line', 'TAP version 13\n')
    Readable.prototype.push.call(this, 'TAP version 13\n')
  }

  this._lineBuf += c
  var lines = this._lineBuf.split('\n')
  this._lineBuf = lines.pop()
  lines.forEach(function (line) {
    if (!line.trim())
      line = ''

    this.emit('line', line + '\n')
  }, this)

  return Readable.prototype.push.call(this, c, e)
}

Test.prototype.bailout = function (message) {
  message = message ? ' # ' + ('' + message).trim() : ''
  message = message.replace(/^( #)+ /, ' # ')
  message = message.replace(/[\r\n]/g, ' ')
  this.push('Bail out!' + message + '\n')
  this._bailedOut = true
  this._ok = false
  this.emit('bailout', message)
  this.push(null)
}

Test.prototype._addMoment = function (which, fn) {
  this[which].push(fn)
}

Test.prototype.beforeEach = function (fn) {
  this._addMoment('_beforeEach', fn)
}

Test.prototype.afterEach = function (fn) {
  this._addMoment('_afterEach', fn)
}

// Add all the asserts
tapAsserts.decorate(Test.prototype)
