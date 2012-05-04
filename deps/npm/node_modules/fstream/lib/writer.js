
module.exports = Writer

var fs = require("graceful-fs")
  , inherits = require("inherits")
  , rimraf = require("rimraf")
  , mkdir = require("mkdirp")
  , path = require("path")
  , umask = process.platform === "win32" ? 0 : process.umask()
  , getType = require("./get-type.js")
  , Abstract = require("./abstract.js")

// Must do this *before* loading the child classes
inherits(Writer, Abstract)

Writer.dirmode = 0777 & (~umask)
Writer.filemode = 0666 & (~umask)

var DirWriter = require("./dir-writer.js")
  , LinkWriter = require("./link-writer.js")
  , FileWriter = require("./file-writer.js")
  , ProxyWriter = require("./proxy-writer.js")

// props is the desired state.  current is optionally the current stat,
// provided here so that subclasses can avoid statting the target
// more than necessary.
function Writer (props, current) {
  var me = this

  if (typeof props === "string") {
    props = { path: props }
  }

  if (!props.path) me.error("Must provide a path", null, true)

  // polymorphism.
  // call fstream.Writer(dir) to get a DirWriter object, etc.
  var type = getType(props)
    , ClassType = Writer

  switch (type) {
    case "Directory":
      ClassType = DirWriter
      break
    case "File":
      ClassType = FileWriter
      break
    case "Link":
    case "SymbolicLink":
      ClassType = LinkWriter
      break
    case null:
      // Don't know yet what type to create, so we wrap in a proxy.
      ClassType = ProxyWriter
      break
  }

  if (!(me instanceof ClassType)) return new ClassType(props)

  // now get down to business.

  Abstract.call(me)

  // props is what we want to set.
  // set some convenience properties as well.
  me.type = props.type
  me.props = props
  me.depth = props.depth || 0
  me.clobber = false === props.clobber ? props.clobber : true
  me.parent = props.parent || null
  me.root = props.root || (props.parent && props.parent.root) || me

  me._path = me.path = path.resolve(props.path)
  if (process.platform === "win32") {
    me.path = me._path = me.path.replace(/\?/g, "_")
    if (me._path.length >= 260) {
      me._swallowErrors = true
      me._path = "\\\\?\\" + me.path.replace(/\//g, "\\")
    }
  }
  me.basename = path.basename(props.path)
  me.dirname = path.dirname(props.path)
  me.linkpath = props.linkpath || null

  props.parent = props.root = null

  // console.error("\n\n\n%s setting size to", props.path, props.size)
  me.size = props.size

  if (typeof props.mode === "string") {
    props.mode = parseInt(props.mode, 8)
  }

  me.readable = false
  me.writable = true

  // buffer until ready, or while handling another entry
  me._buffer = []
  me.ready = false

  // start the ball rolling.
  // this checks what's there already, and then calls
  // me._create() to call the impl-specific creation stuff.
  me._stat(current)
}

// Calling this means that it's something we can't create.
// Just assert that it's already there, otherwise raise a warning.
Writer.prototype._create = function () {
  var me = this
  fs[me.props.follow ? "stat" : "lstat"](me._path, function (er, current) {
    if (er) {
      return me.warn("Cannot create " + me._path + "\n" +
                     "Unsupported type: "+me.type, "ENOTSUP")
    }
    me._finish()
  })
}

Writer.prototype._stat = function (current) {
  var me = this
    , props = me.props
    , stat = props.follow ? "stat" : "lstat"

  if (current) statCb(null, current)
  else fs[stat](me._path, statCb)

  function statCb (er, current) {
    // if it's not there, great.  We'll just create it.
    // if it is there, then we'll need to change whatever differs
    if (er || !current) {
      return create(me)
    }

    me._old = current
    var currentType = getType(current)

    // if it's a type change, then we need to clobber or error.
    // if it's not a type change, then let the impl take care of it.
    if (currentType !== me.type) {
      return rimraf(me._path, function (er) {
        if (er) return me.error(er)
        me._old = null
        create(me)
      })
    }

    // otherwise, just handle in the app-specific way
    // this creates a fs.WriteStream, or mkdir's, or whatever
    create(me)
  }
}

function create (me) {
  // console.error("W create", me._path, Writer.dirmode)

  // XXX Need to clobber non-dirs that are in the way,
  // unless { clobber: false } in the props.
  mkdir(path.dirname(me._path), Writer.dirmode, function (er) {
    // console.error("W created", path.dirname(me._path), er)
    if (er) return me.error(er)
    me._create()
  })
}

Writer.prototype._finish = function () {
  var me = this

  // console.error(" W Finish", me._path, me.size)

  // set up all the things.
  // At this point, we're already done writing whatever we've gotta write,
  // adding files to the dir, etc.
  var todo = 0
  var errState = null
  var done = false

  if (me._old) {
    // the times will almost *certainly* have changed.
    // adds the utimes syscall, but remove another stat.
    me._old.atime = new Date(0)
    me._old.mtime = new Date(0)
    // console.error(" W Finish Stale Stat", me._path, me.size)
    setProps(me._old)
  } else {
    var stat = me.props.follow ? "stat" : "lstat"
    // console.error(" W Finish Stating", me._path, me.size)
    fs[stat](me._path, function (er, current) {
      // console.error(" W Finish Stated", me._path, me.size, current)
      if (er) {
        // if we're in the process of writing out a
        // directory, it's very possible that the thing we're linking to
        // doesn't exist yet (especially if it was intended as a symlink),
        // so swallow ENOENT errors here and just soldier on.
        if (er.code === "ENOENT" &&
            (me.type === "Link" || me.type === "SymbolicLink") &&
            process.platform === "win32") {
          me.ready = true
          me.emit("ready")
          me.emit("end")
          me.emit("close")
          me.end = me._finish = function () {}
          return
        } else return me.error(er)
      }
      setProps(me._old = current)
    })
  }

  return

  function setProps (current) {
    // console.error(" W setprops", me._path)
    // mode
    var wantMode = me.props.mode
      , chmod = me.props.follow || me.type !== "SymbolicLink"
              ? "chmod" : "lchmod"

    if (fs[chmod] && typeof wantMode === "number") {
      wantMode = wantMode & 0777
      todo ++
      // console.error(" W chmod", wantMode.toString(8), me.basename, "\r")
      fs[chmod](me._path, wantMode, next(chmod))
    }

    // uid, gid
    // Don't even try it unless root.  Too easy to EPERM.
    if (process.platform !== "win32" &&
        process.getuid && process.getuid() === 0 &&
        ( typeof me.props.uid === "number" ||
          typeof me.props.gid === "number" )) {
      var chown = (me.props.follow || me.type !== "SymbolicLink")
                ? "chown" : "lchown"
      if (fs[chown]) {
        if (typeof me.props.uid !== "number") me.props.uid = current.uid
        if (typeof me.props.gid !== "number") me.props.gid = current.gid
        if (me.props.uid !== current.uid || me.props.gid !== current.gid) {
          todo ++
          // console.error(" W chown", me.props.uid, me.props.gid, me.basename)
          fs[chown](me._path, me.props.uid, me.props.gid, next("chown"))
        }
      }
    }

    // atime, mtime.
    if (fs.utimes && process.platform !== "win32") {
      var utimes = (me.props.follow || me.type !== "SymbolicLink")
                 ? "utimes" : "lutimes"

      if (utimes === "lutimes" && !fs[utimes]) {
        utimes = "utimes"
      }

      var curA = current.atime
        , curM = current.mtime
        , meA = me.props.atime
        , meM = me.props.mtime

      if (meA === undefined) meA = curA
      if (meM === undefined) meM = curM

      if (!isDate(meA)) meA = new Date(meA)
      if (!isDate(meM)) meA = new Date(meM)

      if (meA.getTime() !== curA.getTime() ||
          meM.getTime() !== curM.getTime()) {
        todo ++
        // console.error(" W utimes", meA, meM, me.basename)
        fs[utimes](me._path, meA, meM, next("utimes"))
      }
    }

    // finally, handle the case if there was nothing to do.
    if (todo === 0) {
      // console.error(" W nothing to do", me.basename)
      next("nothing to do")()
    }
  }

  function next (what) { return function (er) {
    // console.error("   W Finish", what, todo)
    if (errState) return
    if (er) {
      er.fstream_finish_call = what
      return me.error(errState = er)
    }
    if (--todo > 0) return
    if (done) return
    done = true

    // all the props have been set, so we're completely done.
    me.emit("end")
    me.emit("close")
  }}
}

Writer.prototype.pipe = function () {
  this.error("Can't pipe from writable stream")
}

Writer.prototype.add = function () {
  this.error("Cannot add to non-Directory type")
}

Writer.prototype.write = function () {
  return true
}

function objectToString (d) {
  return Object.prototype.toString.call(d)
}

function isDate(d) {
  return typeof d === 'object' && objectToString(d) === '[object Date]';
}

