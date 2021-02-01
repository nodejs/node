const path = require('path')

const libaccess = require('libnpmaccess')
const readPackageJson = require('read-package-json-fast')

const npm = require('./npm.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const usageUtil = require('./utils/usage.js')
const getIdentity = require('./utils/get-identity.js')

const usage = usageUtil(
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

const subcommands = [
  'public',
  'restricted',
  'grant',
  'revoke',
  'ls-packages',
  'ls-collaborators',
  'edit',
  '2fa-required',
  '2fa-not-required',
]

const UsageError = (msg) =>
  Object.assign(new Error(`\nUsage: ${msg}\n\n` + usage), {
    code: 'EUSAGE',
  })

const cmd = (args, cb) =>
  access(args)
    .then(x => cb(null, x))
    .catch(err => err.code === 'EUSAGE'
      ? cb(err.message)
      : cb(err)
    )

const access = async ([cmd, ...args], cb) => {
  const fn = subcommands.includes(cmd) && access[cmd]

  if (!cmd)
    throw UsageError('Subcommand is required.')

  if (!fn)
    throw UsageError(`${cmd} is not a recognized subcommand.`)

  return fn(args, { ...npm.flatOptions })
}

const completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length === 2)
    return cb(null, subcommands)

  switch (argv[2]) {
    case 'grant':
      if (argv.length === 3)
        return cb(null, ['read-only', 'read-write'])
      else
        return cb(null, [])

    case 'public':
    case 'restricted':
    case 'ls-packages':
    case 'ls-collaborators':
    case 'edit':
    case '2fa-required':
    case '2fa-not-required':
    case 'revoke':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

access.public = ([pkg], opts) =>
  modifyPackage(pkg, opts, libaccess.public)

access.restricted = ([pkg], opts) =>
  modifyPackage(pkg, opts, libaccess.restricted)

access.grant = async ([perms, scopeteam, pkg], opts) => {
  if (!perms || (perms !== 'read-only' && perms !== 'read-write'))
    throw UsageError('First argument must be either `read-only` or `read-write`.')

  if (!scopeteam)
    throw UsageError('`<scope:team>` argument is required.')

  const [, scope, team] = scopeteam.match(/^@?([^:]+):(.*)$/) || []

  if (!scope && !team) {
    throw UsageError(
      'Second argument used incorrect format.\n' +
      'Example: @example:developers'
    )
  }

  return modifyPackage(pkg, opts, (pkgName, opts) =>
    libaccess.grant(pkgName, scopeteam, perms, opts), false)
}

access.revoke = async ([scopeteam, pkg], opts) => {
  if (!scopeteam)
    throw UsageError('`<scope:team>` argument is required.')

  const [, scope, team] = scopeteam.match(/^@?([^:]+):(.*)$/) || []

  if (!scope || !team) {
    throw UsageError(
      'First argument used incorrect format.\n' +
      'Example: @example:developers'
    )
  }

  return modifyPackage(pkg, opts, (pkgName, opts) =>
    libaccess.revoke(pkgName, scopeteam, opts))
}

access['2fa-required'] = access.tfaRequired = ([pkg], opts) =>
  modifyPackage(pkg, opts, libaccess.tfaRequired, false)

access['2fa-not-required'] = access.tfaNotRequired = ([pkg], opts) =>
  modifyPackage(pkg, opts, libaccess.tfaNotRequired, false)

access['ls-packages'] = access.lsPackages = async ([owner], opts) => {
  if (!owner)
    owner = await getIdentity(opts)

  const pkgs = await libaccess.lsPackages(owner, opts)

  // TODO - print these out nicely (breaking change)
  output(JSON.stringify(pkgs, null, 2))
}

access['ls-collaborators'] = access.lsCollaborators = async ([pkg, usr], opts) => {
  const pkgName = await getPackage(pkg, false)
  const collabs = await libaccess.lsCollaborators(pkgName, usr, opts)

  // TODO - print these out nicely (breaking change)
  output(JSON.stringify(collabs, null, 2))
}

access.edit = () =>
  Promise.reject(new Error('edit subcommand is not implemented yet'))

const modifyPackage = (pkg, opts, fn, requireScope = true) =>
  getPackage(pkg, requireScope)
    .then(pkgName => otplease(opts, opts => fn(pkgName, opts)))

const getPackage = async (name, requireScope) => {
  if (name && name.trim())
    return name.trim()
  else {
    try {
      const pkg = await readPackageJson(path.resolve(npm.prefix, 'package.json'))
      name = pkg.name
    } catch (err) {
      if (err.code === 'ENOENT') {
        throw new Error(
          'no package name passed to command and no package.json found'
        )
      } else
        throw err
    }

    if (requireScope && !name.match(/^@[^/]+\/.*$/))
      throw UsageError('This command is only available for scoped packages.')
    else
      return name
  }
}

module.exports = Object.assign(cmd, { usage, completion, subcommands })
