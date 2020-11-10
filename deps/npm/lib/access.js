'use strict'
/* eslint-disable standard/no-callback-literal */

const BB = require('bluebird')

const figgyPudding = require('figgy-pudding')
const libaccess = require('libnpm/access')
const npmConfig = require('./config/figgy-config.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const path = require('path')
const prefix = require('./npm.js').prefix
const readPackageJson = BB.promisify(require('read-package-json'))
const usage = require('./utils/usage.js')
const whoami = require('./whoami.js')

module.exports = access

access.usage = usage(
  'npm access',
  'npm access public [<package>]\n' +
  'npm access restricted [<package>]\n' +
  'npm access grant <read-only|read-write> <scope:team> [<package>]\n' +
  'npm access revoke <scope:team> [<package>]\n' +
  'npm access 2fa-required [<package>]\n' +
  'npm access 2fa-not-required [<package>]\n' +
  'npm access ls-packages [<user>|<scope>|<scope:team>]\n' +
  'npm access ls-collaborators [<package> [<user>]]\n' +
  'npm access edit [<package>]'
)

access.subcommands = [
  'public', 'restricted', 'grant', 'revoke',
  'ls-packages', 'ls-collaborators', 'edit',
  '2fa-required', '2fa-not-required'
]

const AccessConfig = figgyPudding({
  json: {}
})

function UsageError (msg = '') {
  throw Object.assign(new Error(
    (msg ? `\nUsage: ${msg}\n\n` : '') +
    access.usage
  ), {code: 'EUSAGE'})
}

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
    case '2fa-required':
    case '2fa-not-required':
      return cb(null, [])
    case 'revoke':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

function access ([cmd, ...args], cb) {
  return BB.try(() => {
    const fn = access.subcommands.includes(cmd) && access[cmd]
    if (!cmd) { UsageError('Subcommand is required.') }
    if (!fn) { UsageError(`${cmd} is not a recognized subcommand.`) }

    return fn(args, AccessConfig(npmConfig()))
  }).then(
    x => cb(null, x),
    err => err.code === 'EUSAGE' ? cb(err.message) : cb(err)
  )
}

access.public = ([pkg], opts) => {
  return modifyPackage(pkg, opts, libaccess.public)
}

access.restricted = ([pkg], opts) => {
  return modifyPackage(pkg, opts, libaccess.restricted)
}

access.grant = ([perms, scopeteam, pkg], opts) => {
  return BB.try(() => {
    if (!perms || (perms !== 'read-only' && perms !== 'read-write')) {
      UsageError('First argument must be either `read-only` or `read-write.`')
    }
    if (!scopeteam) {
      UsageError('`<scope:team>` argument is required.')
    }
    const [, scope, team] = scopeteam.match(/^@?([^:]+):(.*)$/) || []
    if (!scope && !team) {
      UsageError(
        'Second argument used incorrect format.\n' +
        'Example: @example:developers'
      )
    }
    return modifyPackage(pkg, opts, (pkgName, opts) => {
      return libaccess.grant(pkgName, scopeteam, perms, opts)
    }, false)
  })
}

access.revoke = ([scopeteam, pkg], opts) => {
  return BB.try(() => {
    if (!scopeteam) {
      UsageError('`<scope:team>` argument is required.')
    }
    const [, scope, team] = scopeteam.match(/^@?([^:]+):(.*)$/) || []
    if (!scope || !team) {
      UsageError(
        'First argument used incorrect format.\n' +
        'Example: @example:developers'
      )
    }
    return modifyPackage(pkg, opts, (pkgName, opts) => {
      return libaccess.revoke(pkgName, scopeteam, opts)
    })
  })
}

access['2fa-required'] = access.tfaRequired = ([pkg], opts) => {
  return modifyPackage(pkg, opts, libaccess.tfaRequired, false)
}

access['2fa-not-required'] = access.tfaNotRequired = ([pkg], opts) => {
  return modifyPackage(pkg, opts, libaccess.tfaNotRequired, false)
}

access['ls-packages'] = access.lsPackages = ([owner], opts) => {
  return (
    owner ? BB.resolve(owner) : BB.fromNode(cb => whoami([], true, cb))
  ).then(owner => {
    return libaccess.lsPackages(owner, opts)
  }).then(pkgs => {
    // TODO - print these out nicely (breaking change)
    output(JSON.stringify(pkgs, null, 2))
  })
}

access['ls-collaborators'] = access.lsCollaborators = ([pkg, usr], opts) => {
  return getPackage(pkg, false).then(pkgName =>
    libaccess.lsCollaborators(pkgName, usr, opts)
  ).then(collabs => {
    // TODO - print these out nicely (breaking change)
    output(JSON.stringify(collabs, null, 2))
  })
}

access['edit'] = () => BB.reject(new Error('edit subcommand is not implemented yet'))

function modifyPackage (pkg, opts, fn, requireScope = true) {
  return getPackage(pkg, requireScope).then(pkgName =>
    otplease(opts, opts => fn(pkgName, opts))
  )
}

function getPackage (name, requireScope = true) {
  return BB.try(() => {
    if (name && name.trim()) {
      return name.trim()
    } else {
      return readPackageJson(
        path.resolve(prefix, 'package.json')
      ).then(
        data => data.name,
        err => {
          if (err.code === 'ENOENT') {
            throw new Error('no package name passed to command and no package.json found')
          } else {
            throw err
          }
        }
      )
    }
  }).then(name => {
    if (requireScope && !name.match(/^@[^/]+\/.*$/)) {
      UsageError('This command is only available for scoped packages.')
    } else {
      return name
    }
  })
}
