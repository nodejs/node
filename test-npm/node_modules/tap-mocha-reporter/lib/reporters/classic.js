exports = module.exports = Classic

var Base = require('./base')
  , cursor = Base.cursor
  , color = Base.color
  , yaml = require('js-yaml')
  , util = require('util')
  , fancy = Base.useColors && !process.env.TRAVIS
  , ms = require('../ms.js')
  , diff = require('diff')
  , utils = require('../utils.js')
  , uclen = require('unicode-length').get
  , colorSupport = require('color-support')()

function hasOwnProperty (obj, key) {
  return Object.prototype.hasOwnProperty.call(obj, key)
}

function doDiff (found, wanted, palette) {
  // TODO: Make this a configurable thing or something?
  //
  // Choosing a palette for diffs in a test-runner context
  // is really tricky.  The temptation is to make it look
  // exactly like `git diff`, but the default red and green
  // are very confusing with the colors used to indicate
  // pass/fail.
  //
  // So, I decided to experiment with setting a background to
  // distinguish the diff section from the rest of the test
  // output.  The obvious choice, then, was to mimick GitHub's
  // diff styling.
  //
  // The problem there is that, while those of us with an
  // abundance of cones tend to find that palette most pleasing,
  // it's virtually impossible for people with various sorts of
  // red/green colorblindness to distinguish those colors.
  //
  // The resulting option, with a somewhat pink-ish red and a
  // somewhat yellow-ish green, seems to be a pretty good
  // compromise between esthetics and accessibility.  In a poll
  // on twitter, it was the only one that no color-sighted people
  // strongly objected to, and no color-blind people had
  // significant trouble interpreting.  The twitter poll agrees
  // with the results of Sim Daltonism, which showed that this
  // palette was easily distinguishable across all forms of color
  // deficiency.

  // TODO: add a TrueColor one that looks nicer
  palette = colorSupport.has256 ? 6 : 1

  var plain = ''
  var reset = '\u001b[m'

  switch (palette) {
    case 1:
      // most git-like.  r/g, no background, no bold
      var bg = ''
      var removed = '\u001b[31m'
      var added = '\u001b[32m'
      break

    case 2:
      // dark option, maybe confusing with pass/fail colors?
      var bg = '\u001b[48;5;234m'
      var removed = '\u001b[31;1m'
      var added = '\u001b[32;1m'
      break

    case 3:
      // pastel option, most githubby
      var removed = '\u001b[48;5;224m\u001b[38;5;52m'
      var added = '\u001b[48;5;194m\u001b[38;5;22m'
      var plain = '\u001b[38;5;233m'
      var bg = '\u001b[48;5;255m'
      break

    case 4:
      // orange/cyan pastel option, most r/g-colorblind friendly
      var removed = '\u001b[48;5;223m\u001b[38;5;52m'
      var added = '\u001b[48;5;158m\u001b[38;5;22m'
      var plain = '\u001b[38;5;233m'
      var bg = '\u001b[48;5;255m'
      break

    case 5:
      // pastel option, green is bluish, red is just red
      var removed = '\u001b[48;5;224m\u001b[38;5;52m'
      var added = '\u001b[48;5;158m\u001b[38;5;22m'
      var plain = '\u001b[38;5;233m'
      var bg = '\u001b[48;5;255m'
      break

    case 6:
      // pastel option, red is purplish, green is yellowish
      var removed = '\u001b[48;5;218m\u001b[38;5;52m'
      var added = '\u001b[48;5;193m\u001b[38;5;22m'
      var plain = '\u001b[38;5;233m'
      var bg = '\u001b[48;5;255m'
      break

    case 7:
      // pastel option, red is purplish, green is just green
      var removed = '\u001b[48;5;218m\u001b[38;5;52m'
      var added = '\u001b[48;5;194m\u001b[38;5;22m'
      var plain = '\u001b[38;5;233m'
      var bg = '\u001b[48;5;255m'
      break

    case 8:
      // pastel, red and blue
      var removed = '\u001b[48;5;224m\u001b[38;5;52m'
      var added = '\u001b[48;5;189m\u001b[38;5;17m'
      var plain = '\u001b[38;5;233m'
      var bg = '\u001b[48;5;255m'
      break

    case 9:
      // pastel option, red is purplish, green is yellowish
      var removed = '\u001b[48;5;224m\u001b[38;5;52m'
      var added = '\u001b[48;5;193m\u001b[38;5;22m'
      var plain = '\u001b[38;5;233m'
      var bg = '\u001b[48;5;255m'
      break

  }


  var maxLen = process.stdout.columns || 0
  if (maxLen >= 5)
    maxLen -= 5

  if (!Base.useColors) {
    bg = removed = added = reset = plain = ''
    maxLen = 0
  }

  // If they are not strings, or only differ in trailing whitespace,
  // then stringify them so that we can see the difference.
  if (typeof found !== 'string' ||
      typeof wanted !== 'string' ||
      found.trim() === wanted.trim()) {
    found = utils.stringify(found)
    wanted = utils.stringify(wanted)
  }

  var patch = diff.createPatch('', wanted, found)
  //console.error(patch)
  var width = 0
  patch = patch.split('\n').map(function (line, index) {
    if (uclen(line) > width)
      width = Math.min(maxLen, uclen(line))
    if (line.match(/^\=+$/) ||
        line === '\\ No newline at end of file')
      return null
    else
      return line
  }).filter(function (line, i) {
    return line && i > 4
  }).map(function (line) {
    if (uclen(line) < width)
      line += new Array(width - uclen(line) + 1).join(' ')
    return line
  }).map(function (line) {
    if (line.charAt(0) === '+')
      return bg + added + line + reset
    else if (line.charAt(0) === '-')
      return bg + removed + line + reset
    else
      return bg + plain + line + reset
  }).join('\n')

  var pref =
    bg + added + '+++ found' +
      (Base.useColors
        ? new Array(width - '+++ found'.length + 1).join(' ')
        : '') +
      reset + '\n' +
    bg + removed + '--- wanted' +
      (Base.useColors
        ? new Array(width - '--- wanted'.length + 1).join(' ')
        : '') +
      reset + '\n'

  return pref + patch
}

util.inherits(Classic, Base)

function Classic (runner) {
  Base.call(this, runner);

  var self = this

  var grandTotal = 0
  var grandPass = 0

  var bailed = false
  var hadFails = false
  var currentSuite = null
  var tests = []
  var skipped = 0
  var skipMsg = []
  var todo = []
  var fails = []
  var total = 0
  var pass = 0
  var tickDots = 0
  var tickColor = 'checkmark'

  runner.on('bailout', function (bailout, suite) {
    if (currentSuite)
      runner.emit('suite end', currentSuite)
    if (bailed)
      return
    bailed = true
    console.log(Base.color('fail', 'Bail out! ' + bailout))
  })

  runner.on('suite', function (suite) {
    if (!suite.root)
      return

    if (fancy) {
      process.stdout.write(suite.title + ' ')
      tickDots = 0
      tickColor = 'checkmark'
    }

    currentSuite = suite
    tests = []
    todo = []
    fails = []
    skipMsg = []
    skipped = 0
    pass = 0
    total = 0
  })

  runner.on('suite end', function (suite) {
    if (!suite.root)
      return

    if (fancy)
      Base.cursor.beginningOfLine()

    currentSuite = null
    var len = 60
    var title = suite.title || '(unnamed)'
    var num = pass + '/' + total
    var dots = len - uclen(title) - uclen(num) - 2
    if (dots < 3)
      dots = 3
    dots = ' ' + new Array(dots).join('.') + ' '
    if (fails.length)
      num = Base.color('fail', num)
    else if (pass === total)
      num = Base.color('checkmark', num)
    else
      num = Base.color('pending', num)

    var fmt = title + dots + num
    if (suite.duration / total > 250)
      fmt += Base.color('slow', ' ' + ms(Math.round(suite.duration)))

    console.log(fmt)

    if (fails.length) {
      var failMsg = ''
      fails.forEach(function (t) {
        if (t.parent)
          failMsg += t.parent + '\n'
        failMsg += Base.color('fail', 'not ok ' + t.name) + '\n'
        if (t.diag) {
          var printDiff = false
          if (hasOwnProperty(t.diag, 'found') &&
              hasOwnProperty(t.diag, 'wanted')) {
            printDiff = true
            var found = t.diag.found
            var wanted = t.diag.wanted
            failMsg += indent(doDiff(found, wanted), 2) + '\n'
          }

          var o = {}
          var print = false
          for (var i in t.diag) {
            // Don't re-print what we already showed in the diff
            if (printDiff && ( i === 'found' || i === 'wanted'))
              continue
            o[i] = t.diag[i]
            print = true
          }
          if (print)
            failMsg += indent(yaml.safeDump(o), 2) + '\n'
        }
      })
      console.log(indent(failMsg, 2))
    }

    if (todo.length) {
      var todoMsg = ''
      var bullet = Base.color('pending', '~ ')
      todo.forEach(function (t) {
        if (t.todo !== true)
          t.name += ' - ' + Base.color('pending', t.todo)
        todoMsg += bullet + t.name + '\n'
        if (t.diag)
          todoMsg += indent(yaml.safeDump(t.diag), 4) + '\n'
      })
      console.log(indent(todoMsg, 2))
    }

    if (skipped) {
      var fmt = Base.color('skip', indent('Skipped: %d', 2))
      console.log(fmt, skipped)
      if (skipMsg.length)
        console.log(indent(skipMsg.join('\n'), 4))
      console.log('')
    }
  })

  runner.on('test', function (test) {
    total ++
    grandTotal ++
    var t = test.result
    if (fancy && currentSuite) {
      var max = 57 - uclen(currentSuite.title)
      if (max < 3)
        max = 3

      if (tickDots > max) {
        tickDots = 0
        Base.cursor.deleteLine()
        Base.cursor.beginningOfLine();
        process.stdout.write(currentSuite.title + ' ')
      }
      tickDots ++

      if (t.todo &&
          (tickColor === 'checkmark' || tickColor === 'skip'))
        tickColor = 'pending'
      else if (t.skip && tickColor === 'checkmark')
        tickColor = 'skip'
      else if (!t.ok)
        tickColor = 'fail'

      process.stdout.write(Base.color(tickColor, '.'))
    }

    if (t.skip) {
      skipped += 1
      if (t.skip !== true)
        skipMsg.push(t.name + ' ' + Base.color('skip', t.skip))
      else
        skipMsg.push(t.name)
    }
    else if (t.todo)
      todo.push(t)
    else if (!t.ok) {
      t.parent = []
      var p = test.parent
      while (p && p !== currentSuite) {
        var n  = p.title || p.name || p.fullTitle()
        if (n)
          t.parent.unshift(n)
        p = p.parent
      }
      t.parent.shift()
      t.parent = t.parent.join(' > ')
      fails.push(t)
      hadFails = true
    }
    else {
      pass ++
      grandPass ++
    }
  })

  runner.on('end', function () {
    total = grandTotal
    pass = grandPass
    tests = []
    todo = []
    fails = []
    skipMsg = []
    skipped = 0
    if (hadFails)
      fails = [,,,]
    runner.emit('suite end', { title: 'total', root: true })
    self.failures = []
    self.epilogue();

    if (grandTotal === grandPass) {
      console.log(Base.color('checkmark', '\n  ok'))
    }
  })
}

function indent (str, n) {
  var ind = new Array(n + 1).join(' ')
  str = ind + str.split('\n').join('\n' + ind)
  return str.replace(/(\n\s*)+$/, '\n')
}
