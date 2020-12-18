const npm = require('./npm.js')
const fetch = require('npm-registry-fetch')
const otplease = require('./utils/otplease.js')
const npa = require('npm-package-arg')
const semver = require('semver')
const getIdentity = require('./utils/get-identity.js')
const libaccess = require('libnpmaccess')
const usageUtil = require('./utils/usage.js')

const UsageError = () =>
  Object.assign(new Error(`\nUsage: ${usage}`), {
    code: 'EUSAGE',
  })

const usage = usageUtil(
  'deprecate',
  'npm deprecate <pkg>[@<version>] <message>'
)

const completion = (opts, cb) => {
  if (opts.conf.argv.remain.length > 1)
    return cb(null, [])

  return getIdentity(npm.flatOptions).then((username) => {
    return libaccess.lsPackages(username, npm.flatOptions).then((packages) => {
      return Object.keys(packages)
        .filter((name) => packages[name] === 'write' &&
          (opts.conf.argv.remain.length === 0 ||
            name.startsWith(opts.conf.argv.remain[0]))
        )
    })
  }).then((list) => cb(null, list), (err) => cb(err))
}

const cmd = (args, cb) =>
  deprecate(args)
    .then(() => cb())
    .catch(err => cb(err.code === 'EUSAGE' ? err.message : err))

const deprecate = async ([pkg, msg]) => {
  if (!pkg || !msg)
    throw UsageError()

  // fetch the data and make sure it exists.
  const p = npa(pkg)
  // npa makes the default spec "latest", but for deprecation
  // "*" is the appropriate default.
  const spec = p.rawSpec === '' ? '*' : p.fetchSpec

  if (semver.validRange(spec, true) === null)
    throw new Error(`invalid version range: ${spec}`)

  const uri = '/' + p.escapedName
  const packument = await fetch.json(uri, {
    ...npm.flatOptions,
    spec: p,
    query: { write: true },
  })

  Object.keys(packument.versions)
    .filter(v => semver.satisfies(v, spec, { includePrerelease: true }))
    .forEach(v => {
      packument.versions[v].deprecated = msg
    })

  return otplease(npm.flatOptions, opts => fetch(uri, {
    ...opts,
    spec: p,
    method: 'PUT',
    body: packument,
    ignoreBody: true,
  }))
}

module.exports = Object.assign(cmd, { completion, usage })
