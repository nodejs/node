'use strict'

const path = require('path')
const fs = require('graceful-fs')
const chain = require('slide').chain
const mkdir = require('./mkdir.js')
const rm = require('./rm.js')
const inferOwner = require('infer-owner')
const chown = require('./chown.js')

exports = module.exports = {
  link: link,
  linkIfExists: linkIfExists
}

function linkIfExists (from, to, opts, cb) {
  opts.currentIsLink = false
  opts.currentExists = false
  fs.stat(from, function (er) {
    if (er) return cb()
    fs.readlink(to, function (er, fromOnDisk) {
      if (!er || er.code !== 'ENOENT') {
        opts.currentExists = true
      }
      // if the link already exists and matches what we would do,
      // we don't need to do anything
      if (!er) {
        opts.currentIsLink = true
        var toDir = path.dirname(to)
        var absoluteFrom = path.resolve(toDir, from)
        var absoluteFromOnDisk = path.resolve(toDir, fromOnDisk)
        opts.currentTarget = absoluteFromOnDisk
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

  const tasks = [
    [ensureFromIsNotSource, absTarget, to],
    [fs, 'stat', absTarget],
    [clobberLinkGently, from, to, opts],
    [mkdir, path.dirname(to)],
    [fs, 'symlink', target, to, 'junction']
  ]

  if (chown.selfOwner.uid !== 0) {
    chain(tasks, cb)
  } else {
    inferOwner(to).then(owner => {
      tasks.push([chown, to, owner.uid, owner.gid])
      chain(tasks, cb)
    })
  }
}

exports._clobberLinkGently = clobberLinkGently
function clobberLinkGently (from, to, opts, cb) {
  if (opts.currentExists === false) {
    // nothing to clobber!
    opts.log.silly('gently link', 'link does not already exist', {
      link: to,
      target: from
    })
    return cb()
  }

  if (!opts.clobberLinkGently ||
      opts.force === true ||
      !opts.gently ||
      typeof opts.gently !== 'string') {
    opts.log.silly('gently link', 'deleting existing link forcefully', {
      link: to,
      target: from,
      force: opts.force,
      gently: opts.gently,
      clobberLinkGently: opts.clobberLinkGently
    })
    return rm(to, opts, cb)
  }

  if (!opts.currentIsLink) {
    opts.log.verbose('gently link', 'cannot remove, not a link', to)
    // don't delete.  it'll fail with EEXIST when it tries to symlink.
    return cb()
  }

  if (opts.currentTarget.indexOf(opts.gently) === 0) {
    opts.log.silly('gently link', 'delete existing link', to)
    return rm(to, opts, cb)
  } else {
    opts.log.verbose('gently link', 'refusing to delete existing link', {
      link: to,
      currentTarget: opts.currentTarget,
      newTarget: from,
      gently: opts.gently
    })
    return cb()
  }
}
