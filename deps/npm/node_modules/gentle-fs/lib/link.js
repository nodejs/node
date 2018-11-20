'use strict'

const path = require('path')
const fs = require('graceful-fs')
const chain = require('slide').chain
const mkdir = require('mkdirp')
const rm = require('./rm.js')

exports = module.exports = {
  link: link,
  linkIfExists: linkIfExists
}

function linkIfExists (from, to, opts, cb) {
  fs.stat(from, function (er) {
    if (er) return cb()
    fs.readlink(to, function (er, fromOnDisk) {
      // if the link already exists and matches what we would do,
      // we don't need to do anything
      if (!er) {
        var toDir = path.dirname(to)
        var absoluteFrom = path.resolve(toDir, from)
        var absoluteFromOnDisk = path.resolve(toDir, fromOnDisk)
        if (absoluteFrom === absoluteFromOnDisk) return cb()
      }
      link(from, to, opts, cb)
    })
  })
}

function resolveIfSymlink (maybeSymlinkPath, cb) {
  fs.lstat(maybeSymlinkPath, function (err, stat) {
    if (err) return cb.apply(this, arguments)
    if (!stat.isSymbolicLink()) return cb(null, maybeSymlinkPath)
    fs.readlink(maybeSymlinkPath, cb)
  })
}

function ensureFromIsNotSource (from, to, cb) {
  resolveIfSymlink(from, function (err, fromDestination) {
    if (err) return cb.apply(this, arguments)
    if (path.resolve(path.dirname(from), fromDestination) === path.resolve(to)) {
      return cb(new Error('Link target resolves to the same directory as link source: ' + to))
    }
    cb.apply(this, arguments)
  })
}

function link (from, to, opts, cb) {
  to = path.resolve(to)
  opts.base = path.dirname(to)
  var absTarget = path.resolve(opts.base, from)
  var relativeTarget = path.relative(opts.base, absTarget)
  var target = opts.absolute ? absTarget : relativeTarget

  chain(
    [
      [ensureFromIsNotSource, absTarget, to],
      [fs, 'stat', absTarget],
      [rm, to, opts],
      [mkdir, path.dirname(to)],
      [fs, 'symlink', target, to, 'junction']
    ],
    cb
  )
}
