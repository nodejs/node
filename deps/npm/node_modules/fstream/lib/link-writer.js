
module.exports = LinkWriter

var fs = require("graceful-fs")
  , Writer = require("./writer.js")
  , inherits = require("inherits")
  , collect = require("./collect.js")
  , path = require("path")
  , rimraf = require("rimraf")

inherits(LinkWriter, Writer)

function LinkWriter (props) {
  var me = this
  if (!(me instanceof LinkWriter)) throw new Error(
    "LinkWriter must be called as constructor.")

  // should already be established as a Link type
  if (!((props.type === "Link" && props.Link) ||
        (props.type === "SymbolicLink" && props.SymbolicLink))) {
    throw new Error("Non-link type "+ props.type)
  }

  if (!props.linkpath) {
    me.error("Need linkpath property to create " + props.type)
  }

  Writer.call(this, props)
}

LinkWriter.prototype._create = function () {
  var me = this
    , hard = me.type === "Link" || process.platform === "win32"
    , link = hard ? "link" : "symlink"
    , lp = hard ? path.resolve(me.dirname, me.linkpath) : me.linkpath

  // can only change the link path by clobbering
  // For hard links, let's just assume that's always the case, since
  // there's no good way to read them if we don't already know.
  if (hard) return clobber(me, lp, link)

  fs.readlink(me._path, function (er, p) {
    // only skip creation if it's exactly the same link
    if (p && p === lp) return finish(me)
    clobber(me, lp, link)
  })
}

function clobber (me, lp, link) {
  rimraf(me._path, function (er) {
    if (er) return me.error(er)
    create(me, lp, link)
  })
}

function create (me, lp, link) {
  fs[link](lp, me._path, function (er) {
    // if this is a hard link, and we're in the process of writing out a
    // directory, it's very possible that the thing we're linking to
    // doesn't exist yet (especially if it was intended as a symlink),
    // so swallow ENOENT errors here and just soldier in.
    if (er) {
      if (er.code === "ENOENT" && process.platform === "win32") {
        me.ready = true
        me.emit("ready")
        me.emit("end")
        me.emit("close")
        me.end = me._finish = function () {}
      } else return me.error(er)
    }
    finish(me)
  })
}

function finish (me) {
  me.ready = true
  me.emit("ready")
}

LinkWriter.prototype.end = function () {
  this._finish()
}
