
module.exports = read

var buffer = ""
  , tty = require("tty")
  , StringDecoder = require("string_decoder").StringDecoder

function raw (mode) {
  if (process.stdin.setRawMode) {
    if (process.stdin.isTTY) {
      process.stdin.setRawMode(mode)
    }
    return
  }
  // old style
  try {
    tty.setRawMode(mode)
  } catch (e) {}
}


function read (opts, cb) {
  if (!cb) cb = opts, opts = {}

  var p = opts.prompt || ""
    , def = opts.default
    , silent = opts.silent
    , timeout = opts.timeout
    , num = opts.num || null

  if (p && def) p += "("+(silent ? "<default hidden>" : def)+") "

  // switching into raw mode is a little bit painful.
  // avoid if possible.
  var r = silent || num  ? rawRead : normalRead
  if (r === rawRead && !process.stdin.isTTY) r = normalRead

  if (timeout) {
    cb = (function (cb) {
      var called = false
      var t = setTimeout(function () {
        raw(false)
        process.stdout.write("\n")
        if (def) done(null, def)
        else done(new Error("timeout"))
      }, timeout)

      function done (er, data) {
        clearTimeout(t)
        if (called) return
        // stop reading!
        stdin.pause()
        called = true
        cb(er, data)
      }

      return done
    })(cb)
  }

  if (p && !process.stdout.write(p)) {
    process.stdout.on("drain", function D () {
      process.stdout.removeListener("drain", D)
      r(def, timeout, silent, num, cb)
    })
  } else {
    process.nextTick(function () {
      r(def, timeout, silent, num, cb)
    })
  }
}

function normalRead (def, timeout, silent, num, cb) {
  var stdin = process.openStdin()
    , val = ""
    , decoder = new StringDecoder("utf8")

  stdin.resume()
  stdin.on("error", cb)
  stdin.on("data", function D (chunk) {
    // get the characters that are completed.
    val += buffer + decoder.write(chunk)
    buffer = ""

    // \r has no place here.
    val = val.replace(/\r/g, "")

    if (val.indexOf("\n") !== -1) {
      // pluck off any delims at the beginning.
      if (val !== "\n") {
        var i, l
        for (i = 0, l = val.length; i < l; i ++) {
          if (val.charAt(i) !== "\n") break
        }
        if (i !== 0) val = val.substr(i)
      }

      // hack.  if we get the number of chars, just pretend there was a delim
      if (num > 0 && val.length >= num) {
        val = val.substr(0, num) + "\n" + val.substr(num)
      }

      // buffer whatever might have come *after* the delimter
      var delimIndex = val.indexOf("\n")
      if (delimIndex !== -1) {
        buffer = val.substr(delimIndex)
        val = val.substr(0, delimIndex)
      } else {
        buffer = ""
      }

      stdin.pause()
      stdin.removeListener("data", D)
      stdin.removeListener("error", cb)

      // read(1) trims
      val = val.trim() || def
      cb(null, val)
    }
  })
}

function rawRead (def, timeout, silent, num, cb) {
  var stdin = process.openStdin()
    , val = ""
    , decoder = new StringDecoder

  raw(true)
  stdin.resume()
  stdin.on("error", cb)
  stdin.on("data", function D (c) {
    // \r is my enemy.
    var s = decoder.write(c).replace(/\r/g, "\n")
    var i = 0

    LOOP: while (c = s.charAt(i++)) switch (c) {
      case "\u007f": // backspace
        val = val.substr(0, val.length - 1)
        if (!silent) process.stdout.write('\b \b')
        break

      case "\u0004": // EOF
      case "\n":
        raw(false)
        stdin.removeListener("data", D)
        stdin.removeListener("error", cb)
        val = val.trim() || def
        process.stdout.write("\n")
        stdin.pause()
        return cb(null, val)

      case "\u0003": case "\0": // ^C or other signal abort
        raw(false)
        stdin.removeListener("data", D)
        stdin.removeListener("error", cb)
        stdin.pause()
        return cb(new Error("cancelled"))

      default: // just a normal char
        val += buffer + c
        buffer = ""
        if (!silent) process.stdout.write(c)

        // explicitly process a delim if we have enough chars
        // and stop the processing.
        if (num && val.length >= num) D("\n")
        break LOOP
    }
  })
}
