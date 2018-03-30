'use strict'

const PassThrough = require('stream').PassThrough
const path = require('path')
const tar = require('tar')

module.exports = extractStream
module.exports._computeMode = computeMode

function computeMode (fileMode, optMode, umask) {
  return (fileMode | optMode) & ~(umask || 0)
}

function pkgJsonTransform (spec, opts) {
  return entry => {
    if (entry.path === 'package.json') {
      const transformed = new PassThrough()
      let str = ''
      entry.on('end', () => transformed.end(str.replace(
        /}\s*$/,
        `\n,"_resolved": ${
          JSON.stringify(opts.resolved || '')
        }\n,"_integrity": ${
          JSON.stringify(opts.integrity || '')
        }\n,"_from": ${
          JSON.stringify(spec.toString())
        }\n}`
      )))
      entry.on('error', e => transformed.emit('error'))
      entry.on('data', d => { str += d })
      return transformed
    }
  }
}

function extractStream (spec, dest, opts) {
  opts = opts || {}
  const sawIgnores = new Set()
  return tar.x({
    cwd: dest,
    filter: (name, entry) => !entry.header.type.match(/^.*link$/i),
    strip: 1,
    onwarn: msg => opts.log && opts.log.warn('tar', msg),
    uid: opts.uid,
    gid: opts.gid,
    transform: opts.resolved && pkgJsonTransform(spec, opts),
    onentry (entry) {
      if (entry.type.toLowerCase() === 'file') {
        entry.mode = computeMode(entry.mode, opts.fmode, opts.umask)
      } else if (entry.type.toLowerCase() === 'directory') {
        entry.mode = computeMode(entry.mode, opts.dmode, opts.umask)
      } else {
        entry.mode = computeMode(entry.mode, 0, opts.umask)
      }

      // Note: This mirrors logic in the fs read operations that are
      // employed during tarball creation, in the fstream-npm module.
      // It is duplicated here to handle tarballs that are created
      // using other means, such as system tar or git archive.
      if (entry.type.toLowerCase() === 'file') {
        const base = path.basename(entry.path)
        if (base === '.npmignore') {
          sawIgnores.add(entry.path)
        } else if (base === '.gitignore') {
          const npmignore = entry.path.replace(/\.gitignore$/, '.npmignore')
          if (!sawIgnores.has(npmignore)) {
            // Rename, may be clobbered later.
            entry.path = npmignore
          }
        }
      }
    }
  })
}
