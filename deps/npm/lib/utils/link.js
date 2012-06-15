
module.exports = link
link.ifExists = linkIfExists

var fs = require("graceful-fs")
  , chain = require("slide").chain
  , mkdir = require("mkdirp")
  , rm = require("./gently-rm.js")
  , path = require("path")
  , npm = require("../npm.js")

function linkIfExists (from, to, gently, cb) {
  fs.stat(from, function (er) {
    if (er) return cb()
    link(from, to, gently, cb)
  })
}

function link (from, to, gently, cb) {
  if (typeof cb !== "function") cb = gently, gently = null
  if (npm.config.get("force")) gently = false

  chain
    ( [ [fs, "stat", from]
      , [rm, to, gently]
      , [mkdir, path.dirname(to)]
      , [fs, "symlink", from, to, "junction"] ]
    , cb)
}
