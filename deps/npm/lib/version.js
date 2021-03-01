const libversion = require('libnpmversion')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')

const completion = async (opts) => {
  const { conf: { argv: { remain } } } = opts
  if (remain.length > 2)
    return []

  return [
    'major',
    'minor',
    'patch',
    'premajor',
    'preminor',
    'prepatch',
    'prerelease',
    'from-git',
  ]
}

const usage = usageUtil('version',
`npm version [<newversion> | major | minor | patch | premajor | preminor | prepatch | prerelease [--preid=<prerelease-id>] | from-git]
(run in package dir)

'npm -v' or 'npm --version' to print npm version (${npm.version})
'npm view <pkg> version' to view a package's published version
'npm ls' to inspect current package/dependency versions`
)

const cmd = (args, cb) => version(args).then(() => cb()).catch(cb)

const version = async args => {
  switch (args.length) {
    case 0:
      return list()
    case 1:
      return version_(args)
    default:
      throw usage
  }
}

const version_ = async (args) => {
  const prefix = npm.flatOptions.tagVersionPrefix
  const version = await libversion(args[0], {
    ...npm.flatOptions,
    path: npm.prefix,
  })
  return output(`${prefix}${version}`)
}

const list = async () => {
  const results = {}
  const { promisify } = require('util')
  const { resolve } = require('path')
  const readFile = promisify(require('fs').readFile)
  const pj = resolve(npm.prefix, 'package.json')

  const pkg = await readFile(pj, 'utf8')
    .then(data => JSON.parse(data))
    .catch(() => ({}))

  if (pkg.name && pkg.version)
    results[pkg.name] = pkg.version

  results.npm = npm.version
  for (const [key, version] of Object.entries(process.versions))
    results[key] = version

  output(npm.flatOptions.json ? JSON.stringify(results, null, 2) : results)
}

module.exports = Object.assign(cmd, { usage, completion })
