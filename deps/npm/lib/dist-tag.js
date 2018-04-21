/* eslint-disable standard/no-callback-literal */
module.exports = distTag

var log = require('npmlog')
var npa = require('npm-package-arg')
var semver = require('semver')

var npm = require('./npm.js')
var mapToRegistry = require('./utils/map-to-registry.js')
var readLocalPkg = require('./utils/read-local-package.js')
var usage = require('./utils/usage')
var output = require('./utils/output.js')

distTag.usage = usage(
  'dist-tag',
  'npm dist-tag add <pkg>@<version> [<tag>]' +
  '\nnpm dist-tag rm <pkg> <tag>' +
  '\nnpm dist-tag ls [<pkg>]'
)

distTag.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length === 2) {
    return cb(null, ['add', 'rm', 'ls'])
  }

  switch (argv[2]) {
    default:
      return cb()
  }
}

function distTag (args, cb) {
  var cmd = args.shift()
  switch (cmd) {
    case 'add': case 'a': case 'set': case 's':
      return add(args[0], args[1], cb)
    case 'rm': case 'r': case 'del': case 'd': case 'remove':
      return remove(args[1], args[0], cb)
    case 'ls': case 'l': case 'sl': case 'list':
      return list(args[0], cb)
    default:
      return cb('Usage:\n' + distTag.usage)
  }
}

function add (spec, tag, cb) {
  var thing = npa(spec || '')
  var pkg = thing.name
  var version = thing.rawSpec
  var t = (tag || npm.config.get('tag')).trim()

  log.verbose('dist-tag add', t, 'to', pkg + '@' + version)

  if (!pkg || !version || !t) return cb('Usage:\n' + distTag.usage)

  if (semver.validRange(t)) {
    var er = new Error('Tag name must not be a valid SemVer range: ' + t)
    return cb(er)
  }

  fetchTags(pkg, function (er, tags) {
    if (er) return cb(er)

    if (tags[t] === version) {
      log.warn('dist-tag add', t, 'is already set to version', version)
      return cb()
    }
    tags[t] = version

    mapToRegistry(pkg, npm.config, function (er, uri, auth, base) {
      var params = {
        'package': pkg,
        distTag: t,
        version: version,
        auth: auth
      }

      npm.registry.distTags.add(base, params, function (er) {
        if (er) return cb(er)

        output('+' + t + ': ' + pkg + '@' + version)
        cb()
      })
    })
  })
}

function remove (tag, pkg, cb) {
  log.verbose('dist-tag del', tag, 'from', pkg)

  fetchTags(pkg, function (er, tags) {
    if (er) return cb(er)

    if (!tags[tag]) {
      log.info('dist-tag del', tag, 'is not a dist-tag on', pkg)
      return cb(new Error(tag + ' is not a dist-tag on ' + pkg))
    }

    var version = tags[tag]
    delete tags[tag]

    mapToRegistry(pkg, npm.config, function (er, uri, auth, base) {
      var params = {
        'package': pkg,
        distTag: tag,
        auth: auth
      }

      npm.registry.distTags.rm(base, params, function (er) {
        if (er) return cb(er)

        output('-' + tag + ': ' + pkg + '@' + version)
        cb()
      })
    })
  })
}

function list (pkg, cb) {
  if (!pkg) {
    return readLocalPkg(function (er, pkg) {
      if (er) return cb(er)
      if (!pkg) return cb(distTag.usage)
      list(pkg, cb)
    })
  }

  fetchTags(pkg, function (er, tags) {
    if (er) {
      log.error('dist-tag ls', "Couldn't get dist-tag data for", pkg)
      return cb(er)
    }
    var msg = Object.keys(tags).map(function (k) {
      return k + ': ' + tags[k]
    }).sort().join('\n')
    output(msg)
    cb(er, tags)
  })
}

function fetchTags (pkg, cb) {
  mapToRegistry(pkg, npm.config, function (er, uri, auth, base) {
    if (er) return cb(er)

    var params = {
      'package': pkg,
      auth: auth
    }
    npm.registry.distTags.fetch(base, params, function (er, tags) {
      if (er) return cb(er)
      if (!tags || !Object.keys(tags).length) {
        return cb(new Error('No dist-tags found for ' + pkg))
      }

      cb(null, tags)
    })
  })
}
