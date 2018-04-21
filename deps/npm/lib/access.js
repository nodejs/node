'use strict'
/* eslint-disable standard/no-callback-literal */

var resolve = require('path').resolve

var readPackageJson = require('read-package-json')
var mapToRegistry = require('./utils/map-to-registry.js')
var npm = require('./npm.js')
var output = require('./utils/output.js')

var whoami = require('./whoami')

module.exports = access

access.usage =
  'npm access public [<package>]\n' +
  'npm access restricted [<package>]\n' +
  'npm access grant <read-only|read-write> <scope:team> [<package>]\n' +
  'npm access revoke <scope:team> [<package>]\n' +
  'npm access ls-packages [<user>|<scope>|<scope:team>]\n' +
  'npm access ls-collaborators [<package> [<user>]]\n' +
  'npm access edit [<package>]'

access.subcommands = ['public', 'restricted', 'grant', 'revoke',
  'ls-packages', 'ls-collaborators', 'edit']

access.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length === 2) {
    return cb(null, access.subcommands)
  }

  switch (argv[2]) {
    case 'grant':
      if (argv.length === 3) {
        return cb(null, ['read-only', 'read-write'])
      } else {
        return cb(null, [])
      }
    case 'public':
    case 'restricted':
    case 'ls-packages':
    case 'ls-collaborators':
    case 'edit':
      return cb(null, [])
    case 'revoke':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

function access (args, cb) {
  var cmd = args.shift()
  var params
  return parseParams(cmd, args, function (err, p) {
    if (err) { return cb(err) }
    params = p
    return mapToRegistry(params.package, npm.config, invokeCmd)
  })

  function invokeCmd (err, uri, auth, base) {
    if (err) { return cb(err) }
    params.auth = auth
    try {
      return npm.registry.access(cmd, uri, params, function (err, data) {
        if (!err && data) {
          output(JSON.stringify(data, undefined, 2))
        }
        cb(err, data)
      })
    } catch (e) {
      cb(e.message + '\n\nUsage:\n' + access.usage)
    }
  }
}

function parseParams (cmd, args, cb) {
  // mapToRegistry will complain if package is undefined,
  // but it's not needed for ls-packages
  var params = { 'package': '' }
  if (cmd === 'grant') {
    params.permissions = args.shift()
  }
  if (['grant', 'revoke', 'ls-packages'].indexOf(cmd) !== -1) {
    var entity = (args.shift() || '').split(':')
    params.scope = entity[0]
    params.team = entity[1]
  }

  if (cmd === 'ls-packages') {
    if (!params.scope) {
      whoami([], true, function (err, scope) {
        params.scope = scope
        cb(err, params)
      })
    } else {
      cb(null, params)
    }
  } else {
    getPackage(args.shift(), function (err, pkg) {
      if (err) return cb(err)
      params.package = pkg

      if (cmd === 'ls-collaborators') params.user = args.shift()
      cb(null, params)
    })
  }
}

function getPackage (name, cb) {
  if (name && name.trim()) {
    cb(null, name.trim())
  } else {
    readPackageJson(
      resolve(npm.prefix, 'package.json'),
      function (err, data) {
        if (err) {
          if (err.code === 'ENOENT') {
            cb(new Error('no package name passed to command and no package.json found'))
          } else {
            cb(err)
          }
        } else {
          cb(null, data.name)
        }
      }
    )
  }
}
