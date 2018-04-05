'use strict'
// try to find the most reasonable prefix to use

module.exports = findPrefix

const fs = require('fs')
const path = require('path')
const Bluebird = require('bluebird')
const readdir = Bluebird.promisify(fs.readdir)

function findPrefix (dir) {
  return Bluebird.try(() => {
    dir = path.resolve(dir)

    // this is a weird special case where an infinite recurse of
    // node_modules folders resolves to the level that contains the
    // very first node_modules folder
    let walkedUp = false
    while (path.basename(dir) === 'node_modules') {
      dir = path.dirname(dir)
      walkedUp = true
    }
    if (walkedUp) return dir

    return findPrefix_(dir)
  })
}

function findPrefix_ (dir, original) {
  if (!original) original = dir

  const parent = path.dirname(dir)
  // this is a platform independent way of checking if we're in the root
  // directory
  if (parent === dir) return original

  return readdir(dir).then(files => {
    if (files.indexOf('node_modules') !== -1 ||
        files.indexOf('package.json') !== -1) {
      return dir
    }

    return findPrefix_(parent, original)
  }, er => {
    // an error right away is a bad sign.
    // unless the prefix was simply a non
    // existent directory.
    if (er && dir === original && er.code !== 'ENOENT') {
      throw er
    } else {
      return original
    }
  })
}
