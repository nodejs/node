"use strict"
var fs = require("fs")
var path = require("path")
var dz = require("dezalgo")
var npa = require("npm-package-arg")

module.exports = function (spec, where, cb) {
  if (where instanceof Function) { cb = where; where = null }
  if (where == null) where = "."
  cb = dz(cb)
  try {
    var dep = npa(spec)
  }
  catch (e) {
    return cb(e)
  }
  if ((dep.type == "range" || dep.type == "version") && dep.name != dep.raw) return cb(null, dep)
  var specpath = dep.type == "local"
               ? path.resolve(where, dep.spec)
               : path.resolve(dep.rawSpec? dep.rawSpec: dep.name)
  fs.stat(specpath, function (er, s) {
    if (er) return finalize()
    if (!s.isDirectory()) return finalize("local")
    fs.stat(path.join(specpath, "package.json"), function (er) {
      finalize(er ? null : "directory")
    })
  })
  function finalize(type) {
    if (type != null && type != dep.type) {
      dep.type = type
      if (! dep.rawSpec) {
        dep.rawSpec = dep.name
        dep.name = null
      }
    }
    if (dep.type == "local" || dep.type == "directory") dep.spec = specpath
    cb(null, dep)
  }
}
