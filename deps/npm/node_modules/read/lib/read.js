
module.exports = read

var buffer = ""
  , tty = require("tty")
  , StringDecoder = require("string_decoder").StringDecoder

function read (opts, cb) {
  if (!cb) cb = opts, opts = {}

  var p = opts.prompt || ""
    , def = opts.default
    , silent = opts.silent
    , timeout = opts.timeout
    , num = opts.num || null
    , delim = opts.delim || "\n"

  if (p && def) p += "("+(silent ? "<default hidden>" : def)+") "

  // switching into raw mode is a little bit painful.
  // avoid if possible.
  var r = silent || num || delim !== "\n" ? rawRead : normalRead

  if (timeout) {
    cb = (function (cb) {
      var called = false
      var t = setTimeout(function () {
        tty.setRawMode(false)
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
      r(def, timeout, delim, silent, num, cb)
    })
  } else {
    process.nextTick(function () {
      r(def, timeout, delim, silent, num, cb)
    })
  }
}

function normalRead (def, timeout, delim, silent, num, cb) {
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
    // XXX But what if \r is the delim or something dumb like that?
    // Meh.  If anyone complains about this, deal with it.
    val = val.replace(/\r/g, "")

    // TODO Make delim configurable
    if (val.indexOf(delim) !== -1) {
      // pluck off any delims at the beginning.
      if (val !== delim) {
        var i, l
        for (i = 0, l = val.length; i < l; i ++) {
          if (val.charAt(i) !== delim) break
        }
        if (i !== 0) val = val.substr(i)
      }

      // buffer whatever might have come *after* the delimter
      var delimIndex = val.indexOf(delim)
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

function rawRead (def, timeout, delim, silent, num, cb) {
  var stdin = process.openStdin()
    , val = ""
    , decoder = new StringDecoder

  tty.setRawMode(true)
  stdin.resume()
  stdin.on("error", cb)
  stdin.on("data", function D (c) {
    // \r is my enemy.
    c = decoder.write(c).replace(/\r/g, "\n")

    switch (c) {
      case "": // probably just a \r that was ignored.
        break

      case "\u0004": // EOF
      case delim:
        tty.setRawMode(false)
        stdin.removeListener("data", D)
        stdin.removeListener("error", cb)
        val = val.trim() || def
        process.stdout.write("\n")
        stdin.pause()
        return cb(null, val)

      case "\u0003": case "\0": // ^C or other signal abort
        tty.setRawMode(false)
        stdin.removeListener("data", D)
        stdin.removeListener("error", cb)
        stdin.pause()
        return cb(new Error("cancelled"))
        break

      default: // just a normal char
        val += buffer + c
        buffer = ""
        if (!silent) process.stdout.write(c)

        // explicitly process a delim if we have enough chars.
        if (num && val.length >= num) D(delim)
        break
    }
  })
}
