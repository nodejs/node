module.exports = loadPrefix

var findPrefix = require("./find-prefix.js")
var mkdirp = require("mkdirp")
var path = require('path')

function loadPrefix (cb) {
  var cli = this.list[0]

  // try to guess at a good node_modules location.
  var p
    , gp

  if (!Object.prototype.hasOwnProperty.call(cli, "prefix")) {
    p = process.cwd()
  } else {
    p = this.get("prefix")
  }
  gp = this.get("prefix")

  findPrefix(p, function (er, p) {
    Object.defineProperty(this, "localPrefix",
      { get : function () { return p }
      , set : function (r) { return p = r }
      , enumerable : true
      })
    // the prefix MUST exist, or else nothing works.
    if (!this.get("global")) {
      mkdirp(p, next.bind(this))
    } else {
      next.bind(this)(er)
    }
  }.bind(this))

  gp = path.resolve(gp)
  Object.defineProperty(this, "globalPrefix",
    { get : function () { return gp }
    , set : function (r) { return gp = r }
    , enumerable : true
    })
  // the prefix MUST exist, or else nothing works.
  mkdirp(gp, next.bind(this))

  var i = 2
  var errState = null
  function next (er) {
    if (errState) return
    if (er) return cb(errState = er)
    if (--i === 0) return cb()
  }
}
