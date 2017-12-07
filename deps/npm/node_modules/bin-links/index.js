'use strict'

const path = require('path')
const fs = require('graceful-fs')
const linkIfExists = require('gentle-fs').linkIfExists
const cmdShimIfExists = require('cmd-shim').ifExists
const asyncMap = require('slide').asyncMap
const BB = require('bluebird')
const open = BB.promisify(fs.open)
const close = BB.promisify(fs.close)
const stat = BB.promisify(fs.stat)
const chmod = BB.promisify(fs.chmod)
const Transform = require('stream').Transform
const fsWriteStreamAtomic = require('fs-write-stream-atomic')

module.exports = BB.promisify(binLinks)

function binLinks (pkg, folder, global, opts, cb) {
  // if it's global, and folder is in {prefix}/node_modules,
  // then bins are in {prefix}/bin
  // otherwise, then bins are in folder/../.bin
  var parent = pkg.name && pkg.name[0] === '@' ? path.dirname(path.dirname(folder)) : path.dirname(folder)
  var gnm = global && opts.globalDir
  var gtop = parent === gnm

  opts.log.info('linkStuff', opts.pkgId)
  opts.log.silly('linkStuff', opts.pkgId, 'has', parent, 'as its parent node_modules')
  if (global) opts.log.silly('linkStuff', opts.pkgId, 'is part of a global install')
  if (gnm) opts.log.silly('linkStuff', opts.pkgId, 'is installed into a global node_modules')
  if (gtop) opts.log.silly('linkStuff', opts.pkgId, 'is installed into the top-level global node_modules')

  asyncMap(
    [linkBins, linkMans],
    function (fn, cb) {
      if (!fn) return cb()
      opts.log.verbose(fn.name, opts.pkgId)
      fn(pkg, folder, parent, gtop, opts, cb)
    },
    cb
  )
}

function isHashbangFile (file) {
  return open(file, 'r').then((fileHandle) => {
    return new BB((resolve, reject) => {
      fs.read(fileHandle, Buffer.from(new Array(2)), 0, 2, 0, function (err, bytesRead, buffer) {
        close(fileHandle).then(() => {
          resolve(!err && buffer.toString() === '#!')
        }).catch(reject)
      })
    })
  })
}

function dos2Unix (file) {
  return stat(file).then((stats) => {
    let previousChunkEndedInCR = false
    return new BB((resolve, reject) => {
      fs.createReadStream(file)
        .on('error', reject)
        .pipe(new Transform({
          transform: function (chunk, encoding, done) {
            let data = chunk.toString()
            if (previousChunkEndedInCR) {
              data = '\r' + data
            }
            if (data[data.length - 1] === '\r') {
              data = data.slice(0, -1)
              previousChunkEndedInCR = true
            } else {
              previousChunkEndedInCR = false
            }
            done(null, data.replace(/\r\n/g, '\n'))
          },
          flush: function (done) {
            if (previousChunkEndedInCR) {
              this.push('\r')
            }
            done()
          }
        }))
        .on('error', reject)
        .pipe(fsWriteStreamAtomic(file))
        .on('error', reject)
        .on('finish', function () {
          resolve(chmod(file, stats.mode))
        })
    })
  })
}

function getLinkOpts (opts, gently) {
  return Object.assign({}, opts, { gently: gently })
}

function linkBins (pkg, folder, parent, gtop, opts, cb) {
  if (!pkg.bin || (!gtop && path.basename(parent) !== 'node_modules')) {
    return cb()
  }
  var linkOpts = getLinkOpts(opts, gtop && folder)
  var execMode = parseInt('0777', 8) & (~opts.umask)
  var binRoot = gtop ? opts.globalBin
                     : path.resolve(parent, '.bin')
  opts.log.verbose('linkBins', [pkg.bin, binRoot, gtop])

  asyncMap(Object.keys(pkg.bin), function (bin, cb) {
    var dest = path.resolve(binRoot, bin)
    var src = path.resolve(folder, pkg.bin[bin])

    linkBin(src, dest, linkOpts, function (er) {
      if (er) return cb(er)
      // bins should always be executable.
      // XXX skip chmod on windows?
      fs.chmod(src, execMode, function (er) {
        if (er && er.code === 'ENOENT' && opts.ignoreScripts) {
          return cb()
        }
        if (er) return cb(er)
        isHashbangFile(src).then((isHashbang) => {
          if (isHashbang) {
            opts.log.silly('linkBins', 'Converting line endings of hashbang file:', src)
            return dos2Unix(src)
          }
        }).then(() => {
          if (!gtop) return cb()
          var dest = path.resolve(binRoot, bin)
          var out = opts.parseable
                  ? dest + '::' + src + ':BINFILE'
                  : dest + ' -> ' + src

          if (!opts.json && !opts.parseable) {
            opts.log.clearProgress()
            console.log(out)
            opts.log.showProgress()
          }
          cb()
        }).catch(cb)
      })
    })
  }, cb)
}

function linkBin (from, to, opts, cb) {
  if (process.platform !== 'win32') {
    return linkIfExists(from, to, opts, cb)
  } else {
    return cmdShimIfExists(from, to, cb)
  }
}

function linkMans (pkg, folder, parent, gtop, opts, cb) {
  if (!pkg.man || !gtop || process.platform === 'win32') return cb()

  var manRoot = path.resolve(opts.prefix, 'share', 'man')
  opts.log.verbose('linkMans', 'man files are', pkg.man, 'in', manRoot)

  // make sure that the mans are unique.
  // otherwise, if there are dupes, it'll fail with EEXIST
  var set = pkg.man.reduce(function (acc, man) {
    acc[path.basename(man)] = man
    return acc
  }, {})
  pkg.man = pkg.man.filter(function (man) {
    return set[path.basename(man)] === man
  })

  asyncMap(pkg.man, function (man, cb) {
    if (typeof man !== 'string') return cb()
    opts.log.silly('linkMans', 'preparing to link', man)
    var parseMan = man.match(/(.*\.([0-9]+)(\.gz)?)$/)
    if (!parseMan) {
      return cb(new Error(
        man + ' is not a valid name for a man file.  ' +
        'Man files must end with a number, ' +
        'and optionally a .gz suffix if they are compressed.'
      ))
    }

    var stem = parseMan[1]
    var sxn = parseMan[2]
    var bn = path.basename(stem)
    var manSrc = path.resolve(folder, man)
    var manDest = path.join(manRoot, 'man' + sxn, bn)

    linkIfExists(manSrc, manDest, getLinkOpts(opts, gtop && folder), cb)
  }, cb)
}
