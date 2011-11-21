module.exports = Parser
Parser.create = create
Parser.File = File

var tar = require("./tar")
  , Stream = require("stream").Stream
  , fs = require("fs")

function create (cb) {
  return new Parser(cb)
}

var s = 0
  , HEADER = s ++
  , BODY = s ++
  , PAD = s ++

function Parser (cb) {
  this.fields = tar.fields
  this.fieldSize = tar.fieldSize
  this.state = HEADER
  this.position = 0
  this.currentFile = null
  this._header = []
  this._headerPosition = 0
  this._bodyPosition = 0
  this.writable = true
  Stream.apply(this)
  if (cb) this.on("file", cb)
}

Parser.prototype = Object.create(Stream.prototype)

Parser.prototype.write = function (chunk) {
  switch (this.state) {
    case HEADER:
      // buffer up to 512 bytes in memory, and then
      // parse it, emit a "file" event, and stream the rest
      this._header.push(chunk)
      this._headerPosition += chunk.length
      if (this._headerPosition >= tar.headerSize) {
        return this._parseHeader()
      }
      return true

    case BODY:
      // stream it through until the end of the file is reached,
      // and then step over any \0 byte padding.
      var cl = chunk.length
        , bp = this._bodyPosition
        , np = cl + bp
        , s = this.currentFile.size
      if (np < s) {
        this._bodyPosition = np
        return this.currentFile.write(chunk)
      }
      var c = chunk.slice(0, (s - bp))
      this.currentFile.write(c)
      this._closeFile()
      return this.write(chunk.slice(s - bp))

    case PAD:
      for (var i = 0, l = chunk.length; i < l; i ++) {
        if (chunk[i] !== 0) {
          this.state = HEADER
          return this.write(chunk.slice(i))
        }
      }
  }
  return true
}

Parser.prototype.end = function (chunk) {
  if (chunk) this.write(chunk)
  if (this.currentFile) this._closeFile()
  this.emit("end")
  this.emit("close")
}

// at this point, we have at least 512 bytes of header chunks
Parser.prototype._parseHeader = function () {
  var hp = this._headerPosition
    , last = this._header.pop()
    , rem

  if (hp < 512) return this.emit("error", new Error(
    "Trying to parse header before finished"))

  if (hp > 512) {
    var ll = last.length
      , llIntend = 512 - hp + ll
    rem = last.slice(llIntend)
    last = last.slice(0, llIntend)
  }
  this._header.push(last)

  var fields = tar.fields
    , pos = 0
    , field = 0
    , fieldEnds = tar.fieldEnds
    , fieldSize = tar.fieldSize
    , set = {}
    , fpos = 0

  Object.keys(fieldSize).forEach(function (f) {
    set[ fields[f] ] = new Buffer(fieldSize[f])
  })

  this._header.forEach(function (chunk) {
    for (var i = 0, l = chunk.length; i < l; i ++, pos ++, fpos ++) {
      if (pos >= fieldEnds[field]) {
        field ++
        fpos = 0
      }
      // header is null-padded, so when the fields run out,
      // just finish.
      if (null === fields[field]) return
      set[fields[field]][fpos] = chunk[i]
    }
  })

  this._header.length = 0

  // type definitions here:
  // http://cdrecord.berlios.de/private/man/star/star.4.html
  var type = set.TYPE.toString()
    , file = this.currentFile = new File(set)
  if (type === "\0" ||
      type >= "0" && type <= "7") {
    this._addExtended(file)
    this.emit("file", file)
  } else if (type === "g") {
    this._global = this._global || {}
    readPax(this, file, this._global)
  } else if (type === "h" || type === "x" || type === "X") {
    this._extended = this._extended || {}
    readPax(this, file, this._extended)
  } else if (type === "K") {
    this._readLongField(file, "linkname")
  } else if (type === "L") {
    this._readLongField(file, "name")
  }

  this.state = BODY
  if (rem) return this.write(rem)
  return true
}

function readPax (self, file, obj) {
  var buf = ""
  file.on("data", function (c) {
    buf += c
    var lines = buf.split(/\r?\n/)
    buf = lines.pop()
    lines.forEach(function (line) {
      line = line.match(/^[0-9]+ ([^=]+)=(.*)/)
      if (!line) return
      obj[line[1]] = line[2]
    })
  })
}

Parser.prototype._readLongField = function (f, field) {
  var self = this
  this._longFields[field] = ""
  f.on("data", function (c) {
    self._longFields[field] += c
  })
}

Parser.prototype._addExtended = function (file) {
  var g = this._global || {}
    , e = this._extended || {}
  file.extended = {}
  ;[g, e].forEach(function (h) {
    Object.keys(h).forEach(function (k) {
      file.extended[k] = h[k]
      // handle known fields
      switch (k) {
        case "path": file.name = h[k]; break
        case "ctime": file.ctime = new Date(1000 * h[k]); break
        case "mtime": file.mtime = new Date(1000 * h[k]); break
        case "gid": file.gid = parseInt(h[k], 10); break
        case "uid": file.uid = parseInt(h[k], 10); break
        case "charset": file.charset = h[k]; break
        case "gname": file.group = h[k]; break
        case "uname": file.user = h[k]; break
        case "linkpath": file.linkname = h[k]; break
        case "size": file.size = parseInt(h[k], 10); break
        case "SCHILY.devmajor": file.dev.major = parseInt(h[k], 10); break
        case "SCHILY.devminor": file.dev.minor = parseInt(h[k], 10); break
      }
    })
  })
  var lf = this._longFields || {}
  Object.keys(lf).forEach(function (f) {
    file[f] = lf[f]
  })
  this._extended = {}
  this._longFields = {}
}

Parser.prototype._closeFile = function () {
  if (!this.currentFile) return this.emit("error", new Error(
    "Trying to close without current file"))

  this._headerPosition = this._bodyPosition = 0
  this.currentFile.end()
  this.currentFile = null
  this.state = PAD
}


// file stuff

function strF (f) {
  return f.toString("ascii").split("\0").shift() || ""
}

function parse256 (buf) {
  // first byte MUST be either 80 or FF
  // 80 for positive, FF for 2's comp
  var positive
  if (buf[0] === 0x80) positive = true
  else if (buf[0] === 0xFF) positive = false
  else return 0

  if (!positive) {
    // this is rare enough that the string slowness
    // is not a big deal.  You need *very* old files
    // to ever hit this path.
    var s = ""
    for (var i = 1, l = buf.length; i < l; i ++) {
      var byte = buf[i].toString(2)
      if (byte.length < 8) {
        byte = new Array(byte.length - 8 + 1).join("1") + byte
      }
      s += byte
    }
    var ht = s.match(/^([01]*)(10*)$/)
      , head = ht[1]
      , tail = ht[2]
    head = head.split("1").join("2")
               .split("0").join("1")
               .split("2").join("0")
    return -1 * parseInt(head + tail, 2)
  }

  var sum = 0
  for (var i = 1, l = buf.length, p = l - 1; i < l; i ++, p--) {
    sum += buf[i] * Math.pow(256, p)
  }
  return sum
}

function nF (f) {
  if (f[0] & 128 === 128) {
    return parse256(f)
  }
  return parseInt(f.toString("ascii").replace(/\0+/g, "").trim(), 8) || 0
}

function bufferMatch (a, b) {
  if (a.length != b.length) return false
  for (var i = 0, l = a.length; i < l; i ++) {
    if (a[i] !== b[i]) return false
  }
  return true
}

function File (fields) {
  this._raw = fields
  this.name = strF(fields.NAME)
  this.mode = nF(fields.MODE)
  this.uid = nF(fields.UID)
  this.gid = nF(fields.GID)
  this.size = nF(fields.SIZE)
  this.mtime = new Date(nF(fields.MTIME) * 1000)
  this.cksum = nF(fields.CKSUM)
  this.type = strF(fields.TYPE)
  this.linkname = strF(fields.LINKNAME)

  this.ustar = bufferMatch(fields.USTAR, tar.ustar)

  if (this.ustar) {
    this.ustarVersion = nF(fields.USTARVER)
    this.user = strF(fields.UNAME)
    this.group = strF(fields.GNAME)
    this.dev = { major: nF(fields.DEVMAJ)
               , minor: nF(fields.DEVMIN) }
    this.prefix = strF(fields.PREFIX)
    if (this.prefix) {
      this.name = this.prefix + "/" + this.name
    }
  }

  this.writable = true
  this.readable = true
  Stream.apply(this)
}

File.prototype = Object.create(Stream.prototype)

File.types = { File:            "0"
             , HardLink:        "1"
             , SymbolicLink:    "2"
             , CharacterDevice: "3"
             , BlockDevice:     "4"
             , Directory:       "5"
             , FIFO:            "6"
             , ContiguousFile:  "7" }

Object.keys(File.types).forEach(function (t) {
  File.prototype["is"+t] = function () {
    return File.types[t] === this.type
  }
  File.types[ File.types[t] ] = File.types[t]
})

// contiguous files are treated as regular files for most purposes.
File.prototype.isFile = function () {
  return this.type === "0" && this.name.slice(-1) !== "/"
      || this.type === "7"
}

File.prototype.isDirectory = function () {
  return this.type === "5"
      || this.type === "0" && this.name.slice(-1) === "/"
}

File.prototype.write = function (c) {
  this.emit("data", c)
  return true
}

File.prototype.end = function (c) {
  if (c) this.write(c)
  this.emit("end")
  this.emit("close")
}

File.prototype.pause = function () { this.emit("pause") }

File.prototype.resume = function () { this.emit("resume") }
