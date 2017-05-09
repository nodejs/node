'use strict'

const gunzip = require('./util/gunzip-maybe')
const path = require('path')
const pipeline = require('mississippi').pipeline
const tar = require('tar-fs')

module.exports = extractStream
function extractStream (dest, opts) {
  opts = opts || {}
  const sawIgnores = {}
  return pipeline(gunzip(), tar.extract(dest, {
    map: (header) => {
      if (process.platform !== 'win32') {
        header.uid = opts.uid == null ? header.uid : opts.uid
        header.gid = opts.gid == null ? header.gid : opts.gid
      }
      // Note: This mirrors logic in the fs read operations that are
      // employed during tarball creation, in the fstream-npm module.
      // It is duplicated here to handle tarballs that are created
      // using other means, such as system tar or git archive.
      if (header.type === 'file') {
        const base = path.basename(header.name)
        if (base === '.npmignore') {
          sawIgnores[header.name] = true
        } else if (base === '.gitignore') {
          const npmignore = header.name.replace(/\.gitignore$/, '.npmignore')
          if (!sawIgnores[npmignore]) {
            // Rename, may be clobbered later.
            header.name = npmignore
          }
        }
      }
      return header
    },
    ignore: makeIgnore(opts.log),
    dmode: opts.dmode,
    fmode: opts.fmode,
    umask: opts.umask,
    strip: 1
  }))
}

function makeIgnore (log) {
  const sawIgnores = {}
  return (name, header) => _ignore(name, header, sawIgnores, log)
}

function _ignore (name, header, sawIgnores, logger) {
  if (header.type.match(/^.*link$/)) {
    if (logger) {
      logger.warn(
        'extract-stream',
        'excluding symbolic link',
        header.name, '->', header.linkname)
    }
    return true
  }

  return false
}
