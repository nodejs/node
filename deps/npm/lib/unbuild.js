module.exports = unbuild
module.exports.rmStuff = rmStuff
unbuild.usage = 'npm unbuild <folder>\n(this is plumbing)'

const readJson = require('read-package-json')
const gentlyRm = require('./utils/gently-rm.js')
const npm = require('./npm.js')
const path = require('path')
const isInside = require('path-is-inside')
const lifecycle = require('./utils/lifecycle.js')
const asyncMap = require('slide').asyncMap
const chain = require('slide').chain
const log = require('npmlog')
const build = require('./build.js')
const output = require('./utils/output.js')

// args is a list of folders.
// remove any bins/etc, and then delete the folder.
function unbuild (args, silent, cb) {
  if (typeof silent === 'function') {
    cb = silent
    silent = false
  }
  asyncMap(args, unbuild_(silent), cb)
}

function unbuild_ (silent) {
  return function (folder, cb_) {
    function cb (er) {
      cb_(er, path.relative(npm.root, folder))
    }
    folder = path.resolve(folder)
    const base = isInside(folder, npm.prefix) ? npm.prefix : folder
    delete build._didBuild[folder]
    log.verbose('unbuild', folder.substr(npm.prefix.length + 1))
    readJson(path.resolve(folder, 'package.json'), function (er, pkg) {
      // if no json, then just trash it, but no scripts or whatever.
      if (er) return gentlyRm(folder, false, base, cb)
      chain(
        [
          [lifecycle, pkg, 'preuninstall', folder, { failOk: true }],
          [lifecycle, pkg, 'uninstall', folder, { failOk: true }],
          !silent && function (cb) {
            output('unbuild ' + pkg._id)
            cb()
          },
          [rmStuff, pkg, folder],
          [lifecycle, pkg, 'postuninstall', folder, { failOk: true }],
          [gentlyRm, folder, false, base]
        ],
        cb
      )
    })
  }
}

function rmStuff (pkg, folder, cb) {
  // if it's global, and folder is in {prefix}/node_modules,
  // then bins are in {prefix}/bin
  // otherwise, then bins are in folder/../.bin
  const dir = path.dirname(folder)
  const scope = path.basename(dir)
  const parent = scope.charAt(0) === '@' ? path.dirname(dir) : dir
  const gnm = npm.dir
  // gnm might be an absolute path, parent might be relative
  // this checks they're the same directory regardless
  const top = path.relative(gnm, parent) === ''

  log.verbose('unbuild rmStuff', pkg._id, 'from', gnm)
  if (!top) log.verbose('unbuild rmStuff', 'in', parent)
  asyncMap([rmBins, rmMans], function (fn, cb) {
    fn(pkg, folder, parent, top, cb)
  }, cb)
}

function rmBins (pkg, folder, parent, top, cb) {
  if (!pkg.bin) return cb()
  const binRoot = top ? npm.bin : path.resolve(parent, '.bin')
  asyncMap(Object.keys(pkg.bin), function (b, cb) {
    if (process.platform === 'win32') {
      chain([ [gentlyRm, path.resolve(binRoot, b) + '.cmd', true, folder],
        [gentlyRm, path.resolve(binRoot, b), true, folder] ], cb)
    } else {
      gentlyRm(path.resolve(binRoot, b), true, folder, cb)
    }
  }, gentlyRmBinRoot)

  function gentlyRmBinRoot (err) {
    if (err || top) return cb(err)
    return gentlyRm(binRoot, true, parent, cb)
  }
}

function rmMans (pkg, folder, parent, top, cb) {
  if (!pkg.man ||
      !top ||
      process.platform === 'win32' ||
      !npm.config.get('global')) {
    return cb()
  }
  const manRoot = path.resolve(npm.config.get('prefix'), 'share', 'man')
  log.verbose('rmMans', 'man files are', pkg.man, 'in', manRoot)
  asyncMap(pkg.man, function (man, cb) {
    if (Array.isArray(man)) {
      man.forEach(rmMan)
    } else {
      rmMan(man)
    }

    function rmMan (man) {
      log.silly('rmMan', 'preparing to remove', man)
      const parseMan = man.match(/(.*\.([0-9]+)(\.gz)?)$/)
      if (!parseMan) {
        log.error(
          'rmMan', man, 'is not a valid name for a man file.',
          'Man files must end with a number, ' +
          'and optionally a .gz suffix if they are compressed.'
        )
        return cb()
      }

      const stem = parseMan[1]
      const sxn = parseMan[2]
      const gz = parseMan[3] || ''
      const bn = path.basename(stem)
      const manDest = path.join(
        manRoot,
        'man' + sxn,
        (bn.indexOf(pkg.name) === 0 ? bn : pkg.name + '-' + bn) + '.' + sxn + gz
      )
      gentlyRm(manDest, true, cb)
    }
  }, cb)
}
