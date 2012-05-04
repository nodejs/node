// A thing that emits "entry" events with Reader objects
// Pausing it causes it to stop emitting entry events, and also
// pauses the current entry if there is one.

module.exports = DirReader

var fs = require("graceful-fs")
  , fstream = require("../fstream.js")
  , Reader = fstream.Reader
  , inherits = require("inherits")
  , mkdir = require("mkdirp")
  , path = require("path")
  , Reader = require("./reader.js")

inherits(DirReader, Reader)

function DirReader (props) {
  var me = this
  if (!(me instanceof DirReader)) throw new Error(
    "DirReader must be called as constructor.")

  // should already be established as a Directory type
  if (props.type !== "Directory" || !props.Directory) {
    throw new Error("Non-directory type "+ props.type)
  }

  me._entries = null
  me._index = -1
  me._paused = false
  me._length = -1

  Reader.call(this, props)
}

DirReader.prototype._getEntries = function () {
  var me = this
  fs.readdir(me._path, function (er, entries) {
    if (er) return me.error(er)
    me._entries = entries
    me._length = entries.length
    // console.error("DR %s sort =", me.path, me.props.sort)
    if (typeof me.props.sort === "function") {
      me._entries.sort(me.props.sort)
    }
    me._read()
  })
}

// start walking the dir, and emit an "entry" event for each one.
DirReader.prototype._read = function () {
  var me = this

  if (!me._entries) return me._getEntries()

  if (me._paused || me._currentEntry || me._aborted) {
    // console.error("DR paused=%j, current=%j, aborted=%j", me._paused, !!me._currentEntry, me._aborted)
    return
  }

  me._index ++
  if (me._index >= me._length) {
    if (!me._ended) {
      me._ended = true
      me.emit("end")
      me.emit("close")
    }
    return
  }

  // ok, handle this one, then.

  // save creating a proxy, by stat'ing the thing now.
  var p = path.resolve(me._path, me._entries[me._index])
  // set this to prevent trying to _read() again in the stat time.
  me._currentEntry = p
  fs[ me.props.follow ? "stat" : "lstat" ](p, function (er, stat) {
    if (er) return me.error(er)

    var entry = Reader({ path: p
                       , depth: me.depth + 1
                       , root: me.root || me._proxy || me
                       , parent: me._proxy || me
                       , follow: me.follow
                       , filter: me.filter
                       , sort: me.props.sort
                       }, stat)

    // console.error("DR Entry", p, stat.size)

    me._currentEntry = entry

    // "entry" events are for direct entries in a specific dir.
    // "child" events are for any and all children at all levels.
    // This nomenclature is not completely final.

    entry.on("pause", function (who) {
      if (!me._paused) {
        me.pause(who)
      }
    })

    entry.on("resume", function (who) {
      if (me._paused) {
        me.resume(who)
      }
    })

    entry.on("ready", function EMITCHILD () {
      // console.error("DR emit child", entry._path)
      if (me._paused) {
        // console.error("  DR emit child - try again later")
        // pause the child, and emit the "entry" event once we drain.
        // console.error("DR pausing child entry")
        entry.pause(me)
        return me.once("resume", EMITCHILD)
      }

      // skip over sockets.  they can't be piped around properly,
      // so there's really no sense even acknowledging them.
      // if someone really wants to see them, they can listen to
      // the "socket" events.
      if (entry.type === "Socket") {
        me.emit("socket", entry)
      } else {
        me.emit("entry", entry)
        me.emit("child", entry)
      }
    })

    var ended = false
    entry.on("close", onend)
    function onend () {
      if (ended) return
      ended = true
      me.emit("childEnd", entry)
      me.emit("entryEnd", entry)
      me._currentEntry = null
      me._read()
    }

    // XXX Make this work in node.
    // Long filenames should not break stuff.
    entry.on("error", function (er) {
      if (entry._swallowErrors) {
        me.warn(er)
        entry.emit("end")
        entry.emit("close")
      } else {
        me.emit("error", er)
      }
    })

    // proxy up some events.
    ; [ "child"
      , "childEnd"
      , "warn"
      ].forEach(function (ev) {
        entry.on(ev, me.emit.bind(me, ev))
      })
  })
}

DirReader.prototype.pause = function (who) {
  var me = this
  if (me._paused) return
  who = who || me
  me._paused = true
  if (me._currentEntry && me._currentEntry.pause) {
    me._currentEntry.pause(who)
  }
  me.emit("pause", who)
}

DirReader.prototype.resume = function (who) {
  var me = this
  if (!me._paused) return
  who = who || me

  me._paused = false
  // console.error("DR Emit Resume", me._path)
  me.emit("resume", who)
  if (me._paused) {
    // console.error("DR Re-paused", me._path)
    return
  }

  if (me._currentEntry) {
    if (me._currentEntry.resume) {
      me._currentEntry.resume(who)
    }
  } else me._read()
}
