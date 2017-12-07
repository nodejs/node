'use strict'

const BB = require('bluebird')

const assert = require('assert')
const cacache = require('cacache')
const finished = BB.promisify(require('mississippi').finished)
const log = require('npmlog')
const npa = require('npm-package-arg')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const pacote = require('pacote')
const pacoteOpts = require('./config/pacote')
const path = require('path')
const rm = BB.promisify(require('./utils/gently-rm.js'))
const unbuild = BB.promisify(npm.commands.unbuild)

cache.usage = 'npm cache add <tarball file>' +
              '\nnpm cache add <folder>' +
              '\nnpm cache add <tarball url>' +
              '\nnpm cache add <git url>' +
              '\nnpm cache add <name>@<version>' +
              '\nnpm cache clean' +
              '\nnpm cache verify'

cache.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length === 2) {
    return cb(null, ['add', 'clean'])
  }

  // TODO - eventually...
  switch (argv[2]) {
    case 'clean':
    case 'add':
      return cb(null, [])
  }
}

exports = module.exports = cache
function cache (args, cb) {
  const cmd = args.shift()
  let result
  switch (cmd) {
    case 'rm': case 'clear': case 'clean':
      result = clean(args)
      break
    case 'add':
      result = add(args, npm.prefix)
      break
    case 'verify': case 'check':
      result = verify()
      break
    default: return cb('Usage: ' + cache.usage)
  }
  if (!result || !result.then) {
    throw new Error(`npm cache ${cmd} handler did not return a Promise`)
  }
  result.then(() => cb(), cb)
}

// npm cache clean [pkg]*
cache.clean = clean
function clean (args) {
  if (!args) args = []
  if (args.length) {
    return BB.reject(new Error('npm cache clear does not accept arguments'))
  }
  const cachePath = path.join(npm.cache, '_cacache')
  if (!npm.config.get('force')) {
    return BB.reject(new Error("As of npm@5, the npm cache self-heals from corruption issues and data extracted from the cache is guaranteed to be valid. If you want to make sure everything is consistent, use 'npm cache verify' instead.\n\nIf you're sure you want to delete the entire cache, rerun this command with --force."))
  }
  // TODO - remove specific packages or package versions
  return rm(cachePath)
}

// npm cache add <tarball-url>
// npm cache add <pkg> <ver>
// npm cache add <tarball>
// npm cache add <folder>
cache.add = function (pkg, ver, where, scrub) {
  assert(typeof pkg === 'string', 'must include name of package to install')
  if (scrub) {
    return clean([]).then(() => {
      return add([pkg, ver], where)
    })
  }
  return add([pkg, ver], where)
}

function add (args, where) {
  var usage = 'Usage:\n' +
              '    npm cache add <tarball-url>\n' +
              '    npm cache add <pkg>@<ver>\n' +
              '    npm cache add <tarball>\n' +
              '    npm cache add <folder>\n'
  var spec
  log.silly('cache add', 'args', args)
  if (args[1] === undefined) args[1] = null
  // at this point the args length must ==2
  if (args[1] !== null) {
    spec = args[0] + '@' + args[1]
  } else if (args.length === 2) {
    spec = args[0]
  }
  log.verbose('cache add', 'spec', spec)
  if (!spec) return BB.reject(new Error(usage))
  log.silly('cache add', 'parsed spec', spec)
  return finished(pacote.tarball.stream(spec, pacoteOpts({where})).resume())
}

cache.verify = verify
function verify () {
  const cache = path.join(npm.config.get('cache'), '_cacache')
  let prefix = cache
  if (prefix.indexOf(process.env.HOME) === 0) {
    prefix = '~' + prefix.substr(process.env.HOME.length)
  }
  return cacache.verify(cache).then((stats) => {
    output(`Cache verified and compressed (${prefix}):`)
    output(`Content verified: ${stats.verifiedContent} (${stats.keptSize} bytes)`)
    stats.badContentCount && output(`Corrupted content removed: ${stats.badContentCount}`)
    stats.reclaimedCount && output(`Content garbage-collected: ${stats.reclaimedCount} (${stats.reclaimedSize} bytes)`)
    stats.missingContent && output(`Missing content: ${stats.missingContent}`)
    output(`Index entries: ${stats.totalEntries}`)
    output(`Finished in ${stats.runTime.total / 1000}s`)
  })
}

cache.unpack = unpack
function unpack (pkg, ver, unpackTarget, dmode, fmode, uid, gid) {
  return unbuild([unpackTarget], true).then(() => {
    const opts = pacoteOpts({dmode, fmode, uid, gid, offline: true})
    return pacote.extract(npa.resolve(pkg, ver), unpackTarget, opts)
  })
}
