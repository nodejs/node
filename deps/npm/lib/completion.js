module.exports = completion

completion.usage = 'source <(npm completion)'

var npm = require('./npm.js')
var npmconf = require('./config/core.js')
var configDefs = npmconf.defs
var configTypes = configDefs.types
var shorthands = configDefs.shorthands
var nopt = require('nopt')
var configNames = Object.keys(configTypes)
  .filter(function (e) { return e.charAt(0) !== '_' })
var shorthandNames = Object.keys(shorthands)
var allConfs = configNames.concat(shorthandNames)
var once = require('once')
var isWindowsShell = require('./utils/is-windows-shell.js')
var output = require('./utils/output.js')

completion.completion = function (opts, cb) {
  if (opts.w > 3) return cb()

  var fs = require('graceful-fs')
  var path = require('path')
  var bashExists = null
  var zshExists = null
  fs.stat(path.resolve(process.env.HOME, '.bashrc'), function (er) {
    bashExists = !er
    next()
  })
  fs.stat(path.resolve(process.env.HOME, '.zshrc'), function (er) {
    zshExists = !er
    next()
  })
  function next () {
    if (zshExists === null || bashExists === null) return
    var out = []
    if (zshExists) out.push('~/.zshrc')
    if (bashExists) out.push('~/.bashrc')
    if (opts.w === 2) {
      out = out.map(function (m) {
        return ['>>', m]
      })
    }
    cb(null, out)
  }
}

function completion (args, cb) {
  if (isWindowsShell) {
    var e = new Error('npm completion supported only in MINGW / Git bash on Windows')
    e.code = 'ENOTSUP'
    e.errno = require('constants').ENOTSUP // eslint-disable-line node/no-deprecated-api
    return cb(e)
  }

  // if the COMP_* isn't in the env, then just dump the script.
  if (process.env.COMP_CWORD === undefined ||
      process.env.COMP_LINE === undefined ||
      process.env.COMP_POINT === undefined) {
    return dumpScript(cb)
  }

  console.error(process.env.COMP_CWORD)
  console.error(process.env.COMP_LINE)
  console.error(process.env.COMP_POINT)

  // get the partial line and partial word,
  // if the point isn't at the end.
  // ie, tabbing at: npm foo b|ar
  var w = +process.env.COMP_CWORD
  var words = args.map(unescape)
  var word = words[w]
  var line = process.env.COMP_LINE
  var point = +process.env.COMP_POINT
  var partialLine = line.substr(0, point)
  var partialWords = words.slice(0, w)

  // figure out where in that last word the point is.
  var partialWord = args[w]
  var i = partialWord.length
  while (partialWord.substr(0, i) !== partialLine.substr(-1 * i) && i > 0) {
    i--
  }
  partialWord = unescape(partialWord.substr(0, i))
  partialWords.push(partialWord)

  var opts = {
    words: words,
    w: w,
    word: word,
    line: line,
    lineLength: line.length,
    point: point,
    partialLine: partialLine,
    partialWords: partialWords,
    partialWord: partialWord,
    raw: args
  }

  cb = wrapCb(cb, opts)

  console.error(opts)

  if (partialWords.slice(0, -1).indexOf('--') === -1) {
    if (word.charAt(0) === '-') return configCompl(opts, cb)
    if (words[w - 1] &&
        words[w - 1].charAt(0) === '-' &&
        !isFlag(words[w - 1])) {
      // awaiting a value for a non-bool config.
      // don't even try to do this for now
      console.error('configValueCompl')
      return configValueCompl(opts, cb)
    }
  }

  // try to find the npm command.
  // it's the first thing after all the configs.
  // take a little shortcut and use npm's arg parsing logic.
  // don't have to worry about the last arg being implicitly
  // boolean'ed, since the last block will catch that.
  var parsed = opts.conf =
    nopt(configTypes, shorthands, partialWords.slice(0, -1), 0)
  // check if there's a command already.
  console.error(parsed)
  var cmd = parsed.argv.remain[1]
  if (!cmd) return cmdCompl(opts, cb)

  Object.keys(parsed).forEach(function (k) {
    npm.config.set(k, parsed[k])
  })

  // at this point, if words[1] is some kind of npm command,
  // then complete on it.
  // otherwise, do nothing
  cmd = npm.commands[cmd]
  if (cmd && cmd.completion) return cmd.completion(opts, cb)

  // nothing to do.
  cb()
}

function dumpScript (cb) {
  var fs = require('graceful-fs')
  var path = require('path')
  var p = path.resolve(__dirname, 'utils/completion.sh')

  // The Darwin patch below results in callbacks first for the write and then
  // for the error handler, so make sure we only call our callback once.
  cb = once(cb)

  fs.readFile(p, 'utf8', function (er, d) {
    if (er) return cb(er)
    d = d.replace(/^#!.*?\n/, '')

    process.stdout.write(d, function () { cb() })
    process.stdout.on('error', function (er) {
      // Darwin is a pain sometimes.
      //
      // This is necessary because the "source" or "." program in
      // bash on OS X closes its file argument before reading
      // from it, meaning that you get exactly 1 write, which will
      // work most of the time, and will always raise an EPIPE.
      //
      // Really, one should not be tossing away EPIPE errors, or any
      // errors, so casually.  But, without this, `. <(npm completion)`
      // can never ever work on OS X.
      if (er.errno === 'EPIPE') er = null
      cb(er)
    })
  })
}

function unescape (w) {
  if (w.charAt(0) === '\'') return w.replace(/^'|'$/g, '')
  else return w.replace(/\\ /g, ' ')
}

function escape (w) {
  if (!w.match(/\s+/)) return w
  return '\'' + w + '\''
}

// The command should respond with an array.  Loop over that,
// wrapping quotes around any that have spaces, and writing
// them to stdout.  Use console.log, not the outfd config.
// If any of the items are arrays, then join them with a space.
// Ie, returning ['a', 'b c', ['d', 'e']] would allow it to expand
// to: 'a', 'b c', or 'd' 'e'
function wrapCb (cb, opts) {
  return function (er, compls) {
    if (!Array.isArray(compls)) compls = compls ? [compls] : []
    compls = compls.map(function (c) {
      if (Array.isArray(c)) c = c.map(escape).join(' ')
      else c = escape(c)
      return c
    })

    if (opts.partialWord) {
      compls = compls.filter(function (c) {
        return c.indexOf(opts.partialWord) === 0
      })
    }

    console.error([er && er.stack, compls, opts.partialWord])
    if (er || compls.length === 0) return cb(er)

    output(compls.join('\n'))
    cb()
  }
}

// the current word has a dash.  Return the config names,
// with the same number of dashes as the current word has.
function configCompl (opts, cb) {
  var word = opts.word
  var split = word.match(/^(-+)((?:no-)*)(.*)$/)
  var dashes = split[1]
  var no = split[2]
  var flags = configNames.filter(isFlag)
  console.error(flags)

  return cb(null, allConfs.map(function (c) {
    return dashes + c
  }).concat(flags.map(function (f) {
    return dashes + (no || 'no-') + f
  })))
}

// expand with the valid values of various config values.
// not yet implemented.
function configValueCompl (opts, cb) {
  console.error('configValue', opts)
  return cb(null, [])
}

// check if the thing is a flag or not.
function isFlag (word) {
  // shorthands never take args.
  var split = word.match(/^(-*)((?:no-)+)?(.*)$/)
  var no = split[2]
  var conf = split[3]
  return no || configTypes[conf] === Boolean || shorthands[conf]
}

// complete against the npm commands
function cmdCompl (opts, cb) {
  return cb(null, npm.fullList)
}
