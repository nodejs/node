
module.exports = read

var readline = require('readline')
var Mute = require('mute-stream')

function read (opts, cb) {
  if (opts.num) {
    throw new Error('read() no longer accepts a char number limit')
  }

  var input = opts.input || process.stdin
  var output = opts.output || process.stdout
  var m = new Mute({ replace: opts.replace })
  m.pipe(output)
  output = m
  var def = opts.default || ''
  var terminal = !!(opts.terminal || output.isTTY)
  var rlOpts = { input: input, output: output, terminal: terminal }
  var rl = readline.createInterface(rlOpts)
  var prompt = (opts.prompt || '').trim() + ' '
  var silent = opts.silent
  var editDef = false
  var timeout = opts.timeout

  if (def) {
    if (silent) {
      prompt += '(<default hidden>) '
    } else if (opts.edit) {
      editDef = true
    } else {
      prompt += '(' + def + ') '
    }
  }

  output.unmute()
  rl.setPrompt(prompt)
  rl.prompt()
  if (silent) {
    output.mute()
  } else if (editDef) {
    rl.line = def
    rl.cursor = def.length
    rl._refreshLine()
  }

  var called = false
  rl.on('line', onLine)
  rl.on('error', onError)

  rl.on('SIGINT', function () {
    rl.close()
    onError(new Error('canceled'))
  })

  var timer
  if (timeout) {
    timer = setTimeout(function () {
      onError(new Error('timed out'))
    }, timeout)
  }

  function done () {
    called = true
    rl.close()
    clearTimeout(timer)
    output.mute()
  }

  function onError (er) {
    if (called) return
    done()
    return cb(er)
  }

  function onLine (line) {
    if (called) return
    if (silent && terminal) {
      output.unmute()
      output.write('\r\n')
    }
    done()
    // truncate the \n at the end.
    line = line.replace(/\r?\n$/, '')
    var isDefault = !!(editDef && line === def)
    if (def && !line) {
      isDefault = true
      line = def
    }
    cb(null, line, isDefault)
  }
}
