var assert  = require("assert")
var dirname = require("path").dirname
var resolve = require("path").resolve

var rimraf  = require("rimraf")
var lstat   = require("graceful-fs").lstat
var readdir = require("graceful-fs").readdir
var rmdir   = require("graceful-fs").rmdir
var unlink  = require("graceful-fs").unlink

module.exports = vacuum

function vacuum(leaf, options, cb) {
  assert(typeof leaf === "string", "must pass in path to remove")
  assert(typeof cb === "function", "must pass in callback")

  if (!options) options = {}
  assert(typeof options === "object", "options must be an object")

  var log = options.log ? options.log : function () {}

  var base = options.base
  if (base && resolve(leaf).indexOf(resolve(base)) !== 0) {
    return cb(new Error(resolve(leaf) + " is not a child of " + resolve(base)))
  }

  lstat(leaf, function (error, stat) {
    if (error) {
      if (error.code === "ENOENT") return cb(null)

      log(error.stack)
      return cb(error)
    }

    if (!(stat && (stat.isDirectory() || stat.isSymbolicLink() || stat.isFile()))) {
      log(leaf, "is not a directory, file, or link")
      return cb(new Error(leaf + " is not a directory, file, or link"))
    }

    if (options.purge) {
      log("purging", leaf)
      rimraf(leaf, function (error) {
        if (error) return cb(error)

        next(dirname(leaf))
      })
    }
    else if (!stat.isDirectory()) {
      log("removing", leaf)
      unlink(leaf, function (error) {
        if (error) return cb(error)

        next(dirname(leaf))
      })
    }
    else {
      next(leaf)
    }
  })

  function next(branch) {
    // either we've reached the base or we've reached the root
    if ((base && resolve(branch) === resolve(base)) || branch === dirname(branch)) {
      log("finished vacuuming up to", branch)
      return cb(null)
    }

    readdir(branch, function (error, files) {
      if (error) {
        if (error.code === "ENOENT") return cb(null)

        log("unable to check directory", branch, "due to", error.message)
        return cb(error)
      }

      if (files.length > 0) {
        log("quitting because other entries in", branch)
        return cb(null)
      }

      log("removing", branch)
      lstat(branch, function (error, stat) {
        if (error) {
          if (error.code === "ENOENT") return cb(null)

          log("unable to lstat", branch, "due to", error.message)
          return cb(error)
        }

        var remove = stat.isDirectory() ? rmdir : unlink
        remove(branch, function (error) {
          if (error) {
            if (error.code === "ENOENT") return cb(null)

            log("unable to remove", branch, "due to", error.message)
            return cb(error)
          }

          next(dirname(branch))
        })
      })
    })
  }
}
