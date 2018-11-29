'use strict'

const BB = require('bluebird')

const cacache = require('cacache')
const Fetcher = require('../fetch')
const fs = require('fs')
const pipe = BB.promisify(require('mississippi').pipe)
const through = require('mississippi').through

const readFileAsync = BB.promisify(fs.readFile)
const statAsync = BB.promisify(fs.stat)

const MAX_BULK_SIZE = 2 * 1024 * 1024 // 2MB

// `file` packages refer to local tarball files.
const fetchFile = module.exports = Object.create(null)

Fetcher.impl(fetchFile, {
  packument (spec, opts) {
    return BB.reject(new Error('Not implemented yet'))
  },

  manifest (spec, opts) {
    // We can't do much here. `finalizeManifest` will take care of
    // calling `tarball` to fill out all the necessary details.
    return BB.resolve(null)
  },

  // All the heavy lifting for `file` packages is done here.
  // They're never cached. We just read straight out of the file.
  // TODO - maybe they *should* be cached?
  tarball (spec, opts) {
    const src = spec._resolved || spec.fetchSpec
    const stream = through()
    statAsync(src).then(stat => {
      if (spec._resolved) { stream.emit('manifest', spec) }
      if (stat.size <= MAX_BULK_SIZE) {
        // YAY LET'S DO THING IN BULK
        return readFileAsync(src).then(data => {
          if (opts.cache) {
            return cacache.put(
              opts.cache, `pacote:tarball:file:${src}`, data, {
                integrity: opts.integrity
              }
            ).then(integrity => ({ data, integrity }))
          } else {
            return { data }
          }
        }).then(info => {
          if (info.integrity) { stream.emit('integrity', info.integrity) }
          stream.write(info.data, () => {
            stream.end()
          })
        })
      } else {
        let integrity
        const cacheWriter = !opts.cache
          ? BB.resolve(null)
          : (pipe(
            fs.createReadStream(src),
            cacache.put.stream(opts.cache, `pacote:tarball:${src}`, {
              integrity: opts.integrity
            }).on('integrity', d => { integrity = d })
          ))
        return cacheWriter.then(() => {
          if (integrity) { stream.emit('integrity', integrity) }
          return pipe(fs.createReadStream(src), stream)
        })
      }
    }).catch(err => stream.emit('error', err))
    return stream
  },

  fromManifest (manifest, spec, opts) {
    return this.tarball(manifest || spec, opts)
  }
})
