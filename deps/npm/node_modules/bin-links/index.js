'use strict'

const path = require('path')
const fs = require('graceful-fs')
const BB = require('bluebird')
const gentleFs = require('gentle-fs')
const linkIfExists = BB.promisify(gentleFs.linkIfExists)
const gentleFsBinLink = BB.promisify(gentleFs.binLink)
const open = BB.promisify(fs.open)
const close = BB.promisify(fs.close)
const read = BB.promisify(fs.read, {multiArgs: true})
const chmod = BB.promisify(fs.chmod)
const readFile = BB.promisify(fs.readFile)
const writeFileAtomic = BB.promisify(require('write-file-atomic'))
const normalize = require('npm-normalize-package-bin')

module.exports = BB.promisify(binLinks)

function binLinks (pkg, folder, global, opts, cb) {
  pkg = normalize(pkg)
  folder = path.resolve(folder)

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

  return BB.join(
    linkBins(pkg, folder, parent, gtop, opts),
    linkMans(pkg, folder, parent, gtop, opts)
  ).asCallback(cb)
}

function isHashbangFile (file) {
  return open(file, 'r').then(fileHandle => {
    return read(fileHandle, Buffer.alloc(2), 0, 2, 0).spread((_, buf) => {
      if (!hasHashbang(buf)) return []
      return read(fileHandle, Buffer.alloc(2048), 0, 2048, 0)
    }).spread((_, buf) => buf && hasCR(buf), /* istanbul ignore next */ () => false)
      .finally(() => close(fileHandle))
  }).catch(/* istanbul ignore next */ () => false)
}

function hasHashbang (buf) {
  const str = buf.toString()
  return str.slice(0, 2) === '#!'
}

function hasCR (buf) {
  return /^#![^\n]+\r\n/.test(buf)
}

function dos2Unix (file) {
  return readFile(file, 'utf8').then(content => {
    return writeFileAtomic(file, content.replace(/^(#![^\n]+)\r\n/, '$1\n'))
  })
}

function getLinkOpts (opts, gently) {
  return Object.assign({}, opts, { gently: gently })
}

function linkBins (pkg, folder, parent, gtop, opts) {
  if (!pkg.bin || (!gtop && path.basename(parent) !== 'node_modules')) {
    return
  }
  var linkOpts = getLinkOpts(opts, gtop && folder)
  var execMode = parseInt('0777', 8) & (~opts.umask)
  var binRoot = gtop ? opts.globalBin
                     : path.resolve(parent, '.bin')
  opts.log.verbose('linkBins', [pkg.bin, binRoot, gtop])

  return BB.map(Object.keys(pkg.bin), bin => {
    var dest = path.resolve(binRoot, bin)
    var src = path.resolve(folder, pkg.bin[bin])

    /* istanbul ignore if - that unpossible */
    if (src.indexOf(folder) !== 0) {
      throw new Error('invalid bin entry for package ' +
        pkg._id + '. key=' + bin + ', value=' + pkg.bin[bin])
    }

    return linkBin(src, dest, linkOpts).then(() => {
      // bins should always be executable.
      // XXX skip chmod on windows?
      return chmod(src, execMode)
    }).then(() => {
      return isHashbangFile(src)
    }).then(isHashbang => {
      if (!isHashbang) return
      opts.log.silly('linkBins', 'Converting line endings of hashbang file:', src)
      return dos2Unix(src)
    }).then(() => {
      if (!gtop) return
      var dest = path.resolve(binRoot, bin)
      var out = opts.parseable
              ? dest + '::' + src + ':BINFILE'
              : dest + ' -> ' + src

      if (!opts.json && !opts.parseable) {
        opts.log.clearProgress()
        console.log(out)
        opts.log.showProgress()
      }
    }).catch(err => {
      /* istanbul ignore next */
      if (err.code === 'ENOENT' && opts.ignoreScripts) return
      throw err
    })
  })
}

function linkBin (from, to, opts) {
  // do not clobber global bins
  if (opts.globalBin && to.indexOf(opts.globalBin) === 0) {
    opts.clobberLinkGently = true
  }
  return gentleFsBinLink(from, to, opts)
}

function linkMans (pkg, folder, parent, gtop, opts) {
  if (!pkg.man || !gtop || process.platform === 'win32') return

  var manRoot = path.resolve(opts.prefix, 'share', 'man')
  opts.log.verbose('linkMans', 'man files are', pkg.man, 'in', manRoot)

  // make sure that the mans are unique.
  // otherwise, if there are dupes, it'll fail with EEXIST
  var set = pkg.man.reduce(function (acc, man) {
    if (typeof man !== 'string') {
      return acc
    }
    const cleanMan = path.join('/', man).replace(/\\|:/g, '/').substr(1)
    acc[path.basename(man)] = cleanMan
    return acc
  }, {})
  var manpages = pkg.man.filter(function (man) {
    if (typeof man !== 'string') {
      return false
    }
    const cleanMan = path.join('/', man).replace(/\\|:/g, '/').substr(1)
    return set[path.basename(man)] === cleanMan
  })

  return BB.map(manpages, man => {
    opts.log.silly('linkMans', 'preparing to link', man)
    var parseMan = man.match(/(.*\.([0-9]+)(\.gz)?)$/)
    if (!parseMan) {
      throw new Error(
        man + ' is not a valid name for a man file.  ' +
        'Man files must end with a number, ' +
        'and optionally a .gz suffix if they are compressed.'
      )
    }

    var stem = parseMan[1]
    var sxn = parseMan[2]
    var bn = path.basename(stem)
    var manSrc = path.resolve(folder, man)
    /* istanbul ignore if - that unpossible */
    if (manSrc.indexOf(folder) !== 0) {
      throw new Error('invalid man entry for package ' +
        pkg._id + '. man=' + manSrc)
    }

    var manDest = path.join(manRoot, 'man' + sxn, bn)

    // man pages should always be clobbering gently, because they are
    // only installed for top-level global packages, so never destroy
    // a link if it doesn't point into the folder we're linking
    opts.clobberLinkGently = true

    return linkIfExists(manSrc, manDest, getLinkOpts(opts, gtop && folder))
  })
}
