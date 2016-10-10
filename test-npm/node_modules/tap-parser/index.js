// Transforms a stream of TAP into a stream of result objects
// and string comments.  Emits "results" event with summary.
var Writable = require('stream').Writable
/* istanbul ignore if */
if (!Writable) {
  try {
    Writable = require('readable-stream').Writable
  } catch (er) {
    throw new Error('Please install "readable-stream" to use this module ' +
                    'with Node.js v0.8 and before')
  }
}

var yaml = require('js-yaml')
var util = require('util')
var assert = require('assert')

util.inherits(Parser, Writable)

module.exports = Parser

// every line outside of a yaml block is one of these things, or
// a comment, or garbage.
var lineTypes = {
  testPoint: /^(not )?ok(?: ([0-9]+))?(?:(?: -)?( .*))?\n$/,
  pragma: /^pragma ([+-])([a-z]+)\n$/,
  bailout: /^bail out!(.*)\n$/i,
  version: /^TAP version ([0-9]+)\n$/i,
  plan: /^([0-9]+)\.\.([0-9]+)(?:\s+(?:#\s*(.*)))?\n$/,
  subtest: /^# Subtest(?:: (.*))?\n$/,
  subtestIndent: /^    # Subtest(?:: (.*))?\n$/,
  comment: /^\s*#.*\n$/
}

var lineTypeNames = Object.keys(lineTypes)

function lineType (line) {
  for (var t in lineTypes) {
    var match = line.match(lineTypes[t])
    if (match)
      return [t, match]
  }
  return null
}

function parseDirective (line) {
  line = line.trim()
  var time = line.match(/^time=((?:[1-9][0-9]*|0)(?:\.[0-9]+)?)(ms|s)$/i)
  if (time) {
    var n = +time[1]
    if (time[2] === 's') {
      // JS does weird things with floats.  Round it off a bit.
      n *= 1000000
      n = Math.round(n)
      n /= 1000
    }
    return [ 'time', n ]
  }

  var type = line.match(/^(todo|skip)\b/i)
  if (!type)
    return false

  return [ type[1].toLowerCase(), line.substr(type[1].length).trim() || true ]
}

function Result (parsed, count) {
  var ok = !parsed[1]
  var id = +(parsed[2] || count + 1)
  this.ok = ok
  this.id = id

  var rest = parsed[3] || ''
  var name
  rest = rest.replace(/([^\\]|^)((?:\\\\)*)#/g, '$1\n$2').split('\n')
  name = rest.shift()
  rest = rest.filter(function (r) { return r.trim() }).join('#')

  // now, let's see if there's a directive in there.
  var dir = parseDirective(rest.trim())
  if (!dir)
    name += rest ? '#' + rest : ''
  else {
    // handle buffered subtests with todo/skip on them, like
    // ok 1 - bar # todo foo {\n
    var dirKey = dir[0]
    var dirValue = dir[1]
    if (typeof dirValue === 'string' && dirValue.slice(-1) === '{') {
      name += ' {'
      dirValue = dirValue.slice(0, -1).trim()
      if (!dirValue) {
        dirValue = true
      }
    }
    this[dirKey] = dirValue
  }

  if (name)
    this.name = name.trim()

  return this
}

function Parser (options, onComplete) {
  if (typeof options === 'function') {
    onComplete = options
    options = {}
  }

  if (!(this instanceof Parser))
    return new Parser(options, onComplete)

  options = options || {}
  if (onComplete)
    this.on('complete', onComplete)

  this.parent = options.parent || null
  this.sawValidTap = false
  this.failures = []
  this.level = options.level || 0
  Writable.call(this)
  this.buffer = ''
  this.bailedOut = false
  this.planStart = -1
  this.planEnd = -1
  this.planComment = ''
  this.yamlish = ''
  this.yind = ''
  this.child = null
  this.current = null
  this.maybeSubtest = null
  this.extraQueue = []
  this.buffered = options.buffered || null

  this.preserveWhitespace = options.preserveWhitespace || false

  this.count = 0
  this.pass = 0
  this.fail = 0
  this.todo = 0
  this.skip = 0
  this.ok = true

  this.strict = options.strict || false
  this.pragmas = { strict: this.strict }

  this.postPlan = false
}

Parser.prototype.tapError = function (error) {
  this.ok = false
  this.fail ++
  if (typeof error === 'string') {
    error = {
      tapError: error
    }
  }
  this.failures.push(error)
}

Parser.prototype.parseTestPoint = function (testPoint) {
  this.emitResult()

  var res = new Result(testPoint, this.count)
  if (this.planStart !== -1) {
    var lessThanStart = +res.id < this.planStart
    var greaterThanEnd = +res.id > this.planEnd
    if (lessThanStart || greaterThanEnd) {
      if (lessThanStart)
        res.tapError = 'id less than plan start'
      else
        res.tapError = 'id greater than plan end'
      this.tapError(res)
    }
  }

  this.sawValidTap = true
  if (res.id) {
    if (!this.first || res.id < this.first)
      this.first = res.id
    if (!this.last || res.id > this.last)
      this.last = res.id
  }

  // hold onto it, because we might get yamlish diagnostics
  this.current = res
}

Parser.prototype.nonTap = function (data) {
  if (this.strict) {
    this.tapError({
      tapError: 'Non-TAP data encountered in strict mode',
      data: data
    })
    if (this.parent)
      this.parent.nonTap(data)
  }

  if (this.current || this.extraQueue.length)
    this.extraQueue.push(['extra', data])
  else
    this.emit('extra', data)
}

Parser.prototype.plan = function (start, end, comment, line) {
  // not allowed to have more than one plan
  if (this.planStart !== -1) {
    this.nonTap(line)
    return
  }

  // can't put a plan in a child.
  if (this.child || this.yind) {
    this.nonTap(line)
    return
  }

  this.sawValidTap = true
  this.emitResult()

  this.planStart = start
  this.planEnd = end
  var p = { start: start, end: end }
  if (comment)
    this.planComment = p.comment = comment

  // This means that the plan is coming at the END of all the tests
  // Plans MUST be either at the beginning or the very end.  We treat
  // plans like '1..0' the same, since they indicate that no tests
  // will be coming.
  if (this.count !== 0 || this.planEnd === 0)
    this.postPlan = true

  this.emit('plan', p)
}

Parser.prototype.resetYamlish = function () {
  this.yind = ''
  this.yamlish = ''
}

// that moment when you realize it's not what you thought it was
Parser.prototype.yamlGarbage = function () {
  var yamlGarbage = this.yind + '---\n' + this.yamlish
  this.emitResult()
  this.nonTap(yamlGarbage)
}

Parser.prototype.yamlishLine = function (line) {
  if (line === this.yind + '...\n') {
    // end the yaml block
    this.processYamlish()
  } else if (lineTypes.comment.test(line)) {
    this.emitComment(line)
  } else {
    this.yamlish += line
  }
}

Parser.prototype.processYamlish = function () {
  var yamlish = this.yamlish
  this.resetYamlish()

  try {
    var diags = yaml.safeLoad(yamlish)
  } catch (er) {
    this.nonTap(yamlish)
    return
  }

  this.current.diag = diags
  this.emitResult()
}

Parser.prototype.write = function (chunk, encoding, cb) {
  if (typeof encoding === 'string' && encoding !== 'utf8')
    chunk = new Buffer(chunk, encoding)

  if (Buffer.isBuffer(chunk))
    chunk += ''

  if (typeof encoding === 'function') {
    cb = encoding
    encoding = null
  }

  if (this.bailedOut) {
    if (cb)
      process.nextTick(cb)
    return true
  }

  this.buffer += chunk
  do {
    var match = this.buffer.match(/^.*\r?\n/)
    if (!match || this.bailedOut)
      break

    this.buffer = this.buffer.substr(match[0].length)
    this._parse(match[0])
  } while (this.buffer.length)

  if (cb)
    process.nextTick(cb)
  return true
}

Parser.prototype.end = function (chunk, encoding, cb) {
  if (chunk) {
    if (typeof encoding === 'function') {
      cb = encoding
      encoding = null
    }
    this.write(chunk, encoding)
  }

  if (this.buffer)
    this.write('\n')

  // if we have yamlish, means we didn't finish with a ...
  if (this.yamlish)
    this.nonTap(this.yamlish)

  this.emitResult()

  var skipAll

  if (this.planEnd === 0 && this.planStart === 1) {
    skipAll = true
    if (this.count === 0) {
      this.ok = true
    } else {
      this.tapError('Plan of 1..0, but test points encountered')
    }
  } else if (!this.bailedOut && this.planStart === -1) {
    this.tapError('no plan')
  } else if (this.ok && this.count !== (this.planEnd - this.planStart + 1)) {
    this.tapError('incorrect number of tests')
  }

  if (this.ok && !skipAll && this.first !== this.planStart) {
    this.tapError('first test id does not match plan start')
  }

  if (this.ok && !skipAll && this.last !== this.planEnd) {
    this.tapError('last test id does not match plan end')
  }

  var final = {
    ok: this.ok,
    count: this.count,
    pass: this.pass
  }

  if (this.fail)
    final.fail = this.fail

  if (this.bailedOut)
    final.bailout = this.bailedOut

  if (this.todo)
    final.todo = this.todo

  if (this.skip)
    final.skip = this.skip

  if (this.planStart !== -1) {
    final.plan = { start: this.planStart, end: this.planEnd }
    if (skipAll) {
      final.plan.skipAll = true
      if (this.planComment)
        final.plan.skipReason = this.planComment
    }
  }

  // We didn't get any actual tap, so just treat this like a
  // 1..0 test, because it was probably just console.log junk
  if (!this.sawValidTap) {
    final.plan = { start: 1, end: 0 }
    final.ok = true
  }

  if (this.failures.length) {
    final.failures = this.failures
  } else {
    final.failures = []
  }

  this.emit('complete', final)

  Writable.prototype.end.call(this, null, null, cb)
}

Parser.prototype.version = function (version, line) {
  // If version is specified, must be at the very beginning.
  if (version >= 13 && this.planStart === -1 && this.count === 0)
    this.emit('version', version)
  else
    this.nonTap(line)
}

Parser.prototype.pragma = function (key, value, line) {
  // can't put a pragma in a child or yaml block
  if (this.child) {
    this.nonTap(line)
    return
  }

  this.emitResult()
  // only the 'strict' pragma is currently relevant
  if (key === 'strict') {
    this.strict = value
  }
  this.pragmas[key] = value
  this.emit('pragma', key, value)
}

Parser.prototype.bailout = function (reason) {
  this.sawValidTap = true
  this.emitResult()
  this.bailedOut = reason || true
  this.ok = false
  this.emit('bailout', reason)
  if (this.parent)
    this.parent.bailout(reason)
}

Parser.prototype.clearExtraQueue = function () {
  for (var c = 0; c < this.extraQueue.length; c++) {
    this.emit(this.extraQueue[c][0], this.extraQueue[c][1])
  }
  this.extraQueue.length = 0
}

Parser.prototype.endChild = function () {
  if (this.child) {
    this.child.end()
    this.child = null
  }
}

Parser.prototype.emitResult = function () {
  this.endChild()
  this.resetYamlish()

  if (!this.current)
    return this.clearExtraQueue()

  var res = this.current
  this.current = null

  this.count++
  if (res.ok) {
    this.pass++
  } else {
    this.fail++
    if (!res.todo && !res.skip) {
      this.ok = false
      this.failures.push(res)
    }
  }

  if (res.skip)
    this.skip++

  if (res.todo)
    this.todo++

  this.emit('assert', res)
  this.clearExtraQueue()
}

// TODO: We COULD say that any "relevant tap" line that's indented
// by 4 spaces starts a child test, and just call it 'unnamed' if
// it does not have a prefix comment.  In that case, any number of
// 4-space indents can be plucked off to try to find a relevant
// TAP line type, and if so, start the unnamed child.
Parser.prototype.startChild = function (line) {
  var maybeBuffered = this.current && this.current.name && this.current.name.substr(-1) === '{'
  var unindentStream = !maybeBuffered  && this.maybeChild
  var indentStream = !maybeBuffered && !unindentStream &&
    lineTypes.subtestIndent.test(line)
  var unnamed = !maybeBuffered && !unindentStream && !indentStream

  // If we have any other result waiting in the wings, we need to emit
  // that now.  A buffered test emits its test point at the *end* of
  // the child subtest block, so as to match streamed test semantics.
  if (!maybeBuffered)
    this.emitResult()

  this.child = new Parser({
    parent: this,
    level: this.level + 1,
    buffered: maybeBuffered,
    preserveWhitespace: this.preserveWhitespace,
    strict: this.strict
  })

  var self = this
  this.child.on('complete', function (results) {
    if (this.sawValidTap && !results.ok)
      self.ok = false
  })

  // Canonicalize the parsing result of any kind of subtest
  // if it's a buffered subtest or a non-indented Subtest directive,
  // then synthetically emit the Subtest comment
  line = line.substr(4)
  var subtestComment
  if (indentStream) {
    subtestComment = line
    line = null
  } else if (maybeBuffered) {
    var n = this.current.name.trim().replace(/{$/, '').trim()
    subtestComment = '# Subtest: ' + n + '\n'
  } else {
    subtestComment = this.maybeChild || '# Subtest: (anonymous)\n'
  }

  this.maybeChild = null
  this.child.name = subtestComment.substr('# Subtest: '.length).trim()

  // at some point, we may wish to move 100% to preferring
  // the Subtest comment on the parent level.  If so, uncomment
  // this line, and remove the child.emitComment below.
  // this.emit('comment', subtestComment)
  this.emit('child', this.child)
  this.child.emitComment(subtestComment)
  if (line)
    this.child._parse(line)
}

Parser.prototype.emitComment = function (line) {
  if (this.current || this.extraQueue.length)
    this.extraQueue.push(['comment', line])
  else
    this.emit('comment', line)
}

Parser.prototype._parse = function (line) {
  // normalize line endings
  line = line.replace(/\r\n$/, '\n')

  // sometimes empty lines get trimmed, but are still part of
  // a subtest or a yaml block.  Otherwise, nothing to parse!
  if (line === '\n') {
    if (this.child)
      line = '    ' + line
    else if (this.yind)
      line = this.yind + line
  }

  // this is a line we are processing, so emit it
  if (this.preserveWhitespace || line.trim() || this.yind)
    this.emit('line', line)

  if (line === '\n')
    return

  // check to see if the line is indented.
  // if it is, then it's either a subtest, yaml, or garbage.
  var indent = line.match(/^[ \t]*/)[0]
  if (indent) {
    this.parseIndent(line, indent)
    return
  }

  // buffered subtests must end with a }
  if (this.child && this.child.buffered && line === '}\n') {
    this.current.name = this.current.name.replace(/{$/, '').trim()
    this.emitResult()
    return
  }

  // now we know it's not indented, so if it's either valid tap
  // or garbage.  Get the type of line.
  var type = lineType(line)
  if (!type) {
    this.nonTap(line)
    return
  }

  if (type[0] === 'comment') {
    this.emitComment(line)
    return
  }

  // if we have any yamlish, it's garbage now.  We tolerate non-TAP and
  // comments in the midst of yaml (though, perhaps, that's questionable
  // behavior), but any actual TAP means that the yaml block was just
  // not valid.
  if (this.yind)
    this.yamlGarbage()

  // If it's anything other than a comment or garbage, then any
  // maybeChild is just an unsatisfied promise.
  if (this.maybeChild) {
    this.emitComment(this.maybeChild)
    this.maybeChild = null
  }

  // nothing but comments can come after a trailing plan
  if (this.postPlan) {
    this.nonTap(line)
    return
  }

  // ok, now it's maybe a thing
  if (type[0] === 'bailout') {
    this.bailout(type[1][1].trim())
    return
  }

  if (type[0] === 'pragma') {
    var pragma = type[1]
    this.pragma(pragma[2], pragma[1] === '+', line)
    return
  }

  if (type[0] === 'version') {
    var version = type[1]
    this.version(parseInt(version[1], 10), line)
    return
  }

  if (type[0] === 'plan') {
    var plan = type[1]
    this.plan(+plan[1], +plan[2], (plan[3] || '').trim(), line)
    return
  }


  // streamed subtests will end when this test point is emitted
  if (type[0] === 'testPoint') {
    // note: it's weird, but possible, to have a testpoint ending in
    // { before a streamed subtest which ends with a test point
    // instead of a }.  In this case, the parser gets confused, but
    // also, even beginning to handle that means doing a much more
    // involved multi-line parse.  By that point, the subtest block
    // has already been emitted as a 'child' event, so it's too late
    // to really do the optimal thing.  The only way around would be
    // to buffer up everything and do a multi-line parse.  This is
    // rare and weird, and a multi-line parse would be a bigger
    // rewrite, so I'm allowing it as it currently is.
    this.parseTestPoint(type[1])
    return
  }

  // We already detected nontap up above, so the only case left
  // should be a `# Subtest:` comment.  Ignore for coverage, but
  // include the error here just for good measure.
  /* istanbul ignore else */
  if (type[0] === 'subtest') {
    // this is potentially a subtest.  Not indented.
    // hold until later.
    this.maybeChild = line
  } else {
    throw new Error('Unhandled case: ' + type[0])
  }
}

Parser.prototype.parseIndent = function (line, indent) {
  // still belongs to the child, so pass it along.
  if (this.child && line.substr(0, 4) === '    ') {
    line = line.substr(4)
    this.child.write(line)
    return
  }

  // one of:
  // - continuing yaml block
  // - starting yaml block
  // - ending yaml block
  // - body of a new child subtest that was previously introduced
  // - An indented subtest directive
  // - A comment, or garbage

  // continuing/ending yaml block
  if (this.yind) {
    if (line.indexOf(this.yind) === 0) {
      this.yamlishLine(line)
      return
    } else {
      // oops!  that was not actually yamlish, I guess.
      // this is a case where the indent is shortened mid-yamlish block
      // treat existing yaml as garbage, continue parsing this line
      this.yamlGarbage()
    }
  }


  // start a yaml block under a test point
  if (this.current && !this.yind && line === indent + '---\n') {
    this.yind = indent
    return
  }

  // at this point, not yamlish, and not an existing child test.
  // We may have already seen an unindented Subtest directive, or
  // a test point that ended in { indicating a buffered subtest
  // Child tests are always indented 4 spaces.
  if (line.substr(0, 4) === '    ') {
    if (this.maybeChild ||
        this.current && this.current.name && this.current.name.substr(-1) === '{' ||
        lineTypes.subtestIndent.test(line)) {
      this.startChild(line)
      return
    }

    // It's _something_ indented, if the indentation is divisible by
    // 4 spaces, and the result is actual TAP of some sort, then do
    // a child subtest for it as well.
    //
    // This will lead to some ambiguity in cases where there are multiple
    // levels of non-signaled subtests, but a Subtest comment in the
    // middle of them, which may or may not be considered "indented"
    // See the subtest-no-comment-mid-comment fixture for an example
    // of this.  As it happens, the preference is towards an indented
    // Subtest comment as the interpretation, which is the only possible
    // way to resolve this, since otherwise there's no way to distinguish
    // between an anonymous subtest with a non-indented Subtest comment,
    // and an indented Subtest comment.
    var s = line.match(/( {4})+(.*\n)$/)
    if (s[2].charAt(0) !== ' ') {
      // integer number of indentations.
      var type = lineType(s[2])
      if (type) {
        if (type[0] === 'comment') {
          this.emitComment(line)
        } else {
          // it's relevant!  start as an "unnamed" child subtest
          this.startChild(line)
        }
        return
      }
    }
  }

  // at this point, it's either a non-subtest comment, or garbage.
  if (lineTypes.comment.test(line)) {
    this.emitComment(line)
    return
  }

  this.nonTap(line)
}
