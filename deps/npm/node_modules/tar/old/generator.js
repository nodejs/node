module.exports = Generator
Generator.create = create

var tar = require("./tar")
  , Stream = require("stream").Stream
  , Parser = require("./parser")
  , fs = require("fs")

function create (opts) {
  return new Generator(opts)
}

function Generator (opts) {
  this.readable = true
  this.currentFile = null

  this._paused = false
  this._ended = false
  this._queue = []

  this.options = { cwd: process.cwd() }
  Object.keys(opts).forEach(function (o) {
    this.options[o] = opts[o]
  }, this)
  if (this.options.cwd.slice(-1) !== "/") {
    this.options.cwd += "/"
  }

  Stream.apply(this)
}

Generator.prototype = Object.create(Stream.prototype)

Generator.prototype.pause = function () {
  if (this.currentFile) this.currentFile.pause()
  this.paused = true
  this.emit("pause")
}

Generator.prototype.resume = function () {
  this.paused = false
  if (this.currentFile) this.currentFile.resume()
  this.emit("resume")
  this._processQueue()
}

Generator.prototype.end = function () {
  this._ended = true
  this._processQueue()
}

Generator.prototype.append = function (f, st) {
  if (this._ended) return this.emit("error", new Error(
    "Cannot append after ending"))

  // if it's a string, then treat it as a filename.
  // if it's a number, then treat it as a fd
  // if it's a Stats, then treat it as a stat object
  // if it's a Stream, then stream it in.
  var s = toFileStream(f, st)
  if (!s) return this.emit("error", new TypeError(
    "Invalid argument: "+f))

  // make sure it's in the folder being added.
  if (s.name.indexOf(this.options.cwd) !== 0) {
    this.emit("error", new Error(
      "Invalid argument: "+s.name+"\nOutside of "+this.options.cwd))
  }

  s.name = s.name.substr(this.options.cwd.length)
  s.pause()
  this._queue.push(s)

  if (!s._needStat) return this._processQueue()

  var self = this
  fs.lstat(s.name, function (er, st) {
    if (er) return self.emit("error", new Error(
      "invalid file "+s.name+"\n"+er.message))
    s.mode = st.mode & 0777
    s.uid = st.uid
    s.gid = st.gid
    s.size = st.size
    s.mtime = +st.mtime / 1000
    s.type = st.isFile()            ? "0"
           : st.isSymbolicLink()    ? "2"
           : st.isCharacterDevice() ? "3"
           : st.isBlockDevice()     ? "4"
           : st.isDirectory()       ? "5"
           : st.isFIFO()            ? "6"
           : null

    // TODO: handle all the types in
    // http://cdrecord.berlios.de/private/man/star/star.4.html
    // for now, skip over unknown ones.
    if (s.type === null) {
      console.error("Unknown file type: " + s.name)
      // kick out of the queue
      var i = self._queue.indexOf(s)
      if (i !== -1) self._queue.splice(i, 1)
      self._processQueue()
      return
    }

    if (s.type === "2") return fs.readlink(s.name, function (er, n) {
      if (er) return self.emit("error", new Error(
        "error reading link value "+s.name+"\n"+er.message))
      s.linkname = n
      s._needStat = false
      self._processQueue()
    })
    s._needStat = false
    self._processQueue()
  })
  return false
}

function toFileStream (thing) {
  if (typeof thing === "string") {
    return toFileStream(fs.createReadStream(thing))
  }

  if (thing && typeof thing === "object") {
    if (thing instanceof (Parser.File)) return thing

    if (thing instanceof Stream) {
      if (thing.hasOwnProperty("name")  &&
          thing.hasOwnProperty("mode")  &&
          thing.hasOwnProperty("uid")   &&
          thing.hasOwnProperty("gid")   &&
          thing.hasOwnProperty("size")  &&
          thing.hasOwnProperty("mtime") &&
          thing.hasOwnProperty("type")) return thing

      if (thing instanceof (fs.ReadStream)) {
        thing.name = thing.path
      }

      if (thing.name) {
        thing._needStat = true
        return thing
      }
    }
  }

  return null
}

Generator.prototype._processQueue = function processQueue () {
  console.error("processQueue", this._queue[0])
  if (this._paused) return false

  if (this.currentFile ||
      this._queue.length && this._queue[0]._needStat) {
    // either already processing one, or waiting on something.
    return
  }

  var f = this.currentFile = this._queue.shift()
  if (!f) {
    if (this._ended) {
      // close it off with 2 blocks of nulls.
      this.emit("data", new Buffer(new Array(512 * 2)))
      this.emit("end")
      this.emit("close")
    }
    return true
  }

  if (f.type === Parser.File.types.Directory &&
      f.name.slice(-1) !== "/") {
    f.name += "/"
  }

  // write out a Pax header if the file isn't kosher.
  if (this._needPax(f)) this._emitPax(f)

  // write out the header
  f.ustar = true
  this._emitHeader(f)
  var fpos = 0
    , self = this
  console.error("about to read body data", f)
  f.on("data", function (c) {
    self.emit("data", c)
    self.fpos += c.length
  })
  f.on("error", function (er) { self.emit("error", er) })
  f.on("end", function $END () {
    // pad with \0 out to an even multiple of 512 bytes.
    // this ensures that every file starts on a block.
    var b = new Buffer(fpos % 512 || 512)

    for (var i = 0, l = b.length; i < l; i ++) b[i] = 0
    //console.log(b.length, b)
    self.emit("data", b)
    self.currentFile = null
    self._processQueue()
  })
  f.resume()
}

Generator.prototype._needPax = function (f) {
  // meh.  why not?
  return true

  return oddTextField(f.name, "NAME") ||
         oddTextField(f.link, "LINK") ||
         oddTextField(f.gname, "GNAME") ||
         oddTextField(f.uname, "UNAME") ||
         oddTextField(f.prefix, "PREFIX")
}

// check if a text field is too long or non-ascii
function oddTextField (val, field) {
  var nl = Buffer.byteLength(val)
    , len = tar.fieldSize[field]
  if (nl > len || nl !== val.length) return true
}

// emit a Pax header of "key = val" for any file with
// odd or too-long field values.
Generator.prototype._emitPax = function (f) {
  // since these tend to be relatively small, just go ahead
  // and emit it all in-band.  That saves having to keep
  // track of the pax state in the generator, and we can
  // go right back to emitting the file in the same tick.
  var dir = f.name.replace(/[^\/]+\/?$/, "")
    , base = f.name.substr(dir.length)
  var pax = { name: dir + "PaxHeader/" +base
            , mode: 0644
            , uid: f.uid
            , gid: f.gid
            , mtime: +f.mtime
            // don't know size yet.
            , size: -1
            , type: "x" // extended header
            , ustar: true
            , ustarVersion: "00"
            , user: f.user || f.uname || ""
            , group: f.group || f.gname || ""
            , dev: { major: f.dev && f.dev.major || 0
                   , minor: f.dev && f.dev.minor || 0 }
            , prefix: f.prefix
            , linkname: "" }

  // generate the Pax body
  var kv = { path: (f.prefix ? f.prefix + "/" : "") + f.name
           , atime: f.atime
           , mtime: f.mtime
           , ctime: f.ctime
           , charset: "UTF-8"
           , gid: f.gid
           , uid: f.uid
           , uname: f.user || f.uname || ""
           , gname: f.group || f.gname || ""
           , linkpath: f.linkpath || ""
           , size: f.size
           }
  // "%d %s=%s\n", <length>, <keyword>, <value>
  // length includes the length of the length number,
  // the key=val, and the \n.
  var body = new Buffer(Object.keys(kv).map(function (key) {
    if (!kv[key]) return ["", ""]

    var s = new Buffer(" " + key + "=" + kv[key]+"\n")
      , digits = Math.floor(Math.log(s.length) / Math.log(10)) + 1

    // if adding that many digits will make it go over that length,
    // then add one to it
    if (s.length > Math.pow(10, digits) - digits) digits ++

    return [s.length + digits, s]
  }).reduce(function (l, r) {
    return l + r[0] + r[1]
  }, ""))

  pax.size = body.length
  this._emitHeader(pax)
  this.emit("data", body)
  // now the trailing buffer to make it an even number of 512 blocks
  var b = new Buffer(512 + (body.length % 512 || 512))
  for (var i = 0, l = b.length; i < l; i ++) b[i] = 0
  this.emit("data", b)
}

Generator.prototype._emitHeader = function (f) {
  var header = new Buffer(new Array(512))
    , fields = tar.fields
    , offs = tar.fieldOffs
    , sz = tar.fieldSize

  addField(header, "NAME", f.name)
  addField(header, "MODE", f.mode)
  addField(header, "UID", f.uid)
  addField(header, "GID", f.gid)
  addField(header, "SIZE", f.size)
  addField(header, "MTIME", +f.mtime)
  // checksum is generated based on it being spaces
  // then it's written as: "######\0 "
  // where ### is a zero-lead 6-digit octal number
  addField(header, "CKSUM", "        ")

  addField(header, "TYPE", f.type)
  addField(header, "LINKNAME", f.linkname || "")
  if (f.ustar) {
    console.error(">>> ustar!!")
    addField(header, "USTAR", tar.ustar)
    addField(header, "USTARVER", 0)
    addField(header, "UNAME", f.user || "")
    addField(header, "GNAME", f.group || "")
    if (f.dev) {
      addField(header, "DEVMAJ", f.dev.major || 0)
      addField(header, "DEVMIN", f.dev.minor || 0)
    }
    addField(header, "PREFIX", f.prefix)
  } else {
    console.error(">>> no ustar!")
  }

  // now the header is written except for checksum.
  var ck = 0
  for (var i = 0; i < 512; i ++) ck += header[i]
  addField(header, "CKSUM", nF(ck, 7))
  header[ offs[fields.CKSUM] + 7 ] = 0

  this.emit("data", header)
}

function addField (buf, field, val) {
  var f = tar.fields[field]
  console.error("Adding field", field, val)
  val = typeof val === "number"
      ? nF(val, tar.fieldSize[f])
      : new Buffer(val || "")
  val.copy(buf, tar.fieldOffs[f])
}

function toBase256 (num, len) {
  console.error("toBase256", num, len)
  var positive = num > 0
    , buf = new Buffer(len)
  if (!positive) {
    // rare and slow
    var b = num.toString(2).substr(1)
      , padTo = (len - 1) * 8
    b = new Array(padTo - b.length + 1).join("0") + b

    // take the 2's complement
    var ht = b.match(/^([01]*)(10*)?$/)
      , head = ht[1]
      , tail = ht[2]
    head = head.split("1").join("2")
               .split("0").join("1")
               .split("2").join("0")
    b = head + tail

    buf[0] = 0xFF
    for (var i = 1; i < len; i ++) {
      buf[i] = parseInt(buf.substr(i * 8, 8), 2)
    }
    return buf
  }

  buf[0] = 0x80
  for (var i = 1, l = len, p = l - 1; i < l; i ++, p --) {
    buf[p] = num % 256
    num = Math.floor(num / 256)
  }
  return buf
}

function nF (num, size) {
  var ns = num.toString(8)

  if (num < 0 || ns.length >= size) {
    // make a base 256 buffer
    // then return it
    return toBase256(num, size)
  }

  var buf = new Buffer(size)
  ns = new Array(size - ns.length - 1).join("0") + ns + " "
  buf[size - 1] = 0
  buf.asciiWrite(ns)
  return buf
}
