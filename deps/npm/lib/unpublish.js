const path = require('path')
const util = require('util')
const log = require('npmlog')
const npa = require('npm-package-arg')
const libaccess = require('libnpmaccess')
const npmFetch = require('npm-registry-fetch')
const libunpub = require('libnpmpublish').unpublish
const readJson = util.promisify(require('read-package-json'))

const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const getIdentity = require('./utils/get-identity.js')

const usage = usageUtil('unpublish', 'npm unpublish [<@scope>/]<pkg>[@<version>]')

const cmd = (args, cb) => unpublish(args).then(() => cb()).catch(cb)

const completion = (args, cb) => completionFn(args)
  .then((res) => cb(null, res))
  .catch(cb)

const completionFn = async (args) => {
  const { partialWord, conf } = args

  if (conf.argv.remain.length >= 3)
    return []

  const opts = npm.flatOptions
  const username = await getIdentity({ ...opts }).catch(() => null)
  if (!username)
    return []

  const access = await libaccess.lsPackages(username, opts)
  // do a bit of filtering at this point, so that we don't need
  // to fetch versions for more than one thing, but also don't
  // accidentally a whole project
  let pkgs = Object.keys(access || {})
  if (!partialWord || !pkgs.length)
    return pkgs

  const pp = npa(partialWord).name
  pkgs = pkgs.filter(p => !p.indexOf(pp))
  if (pkgs.length > 1)
    return pkgs

  const json = await npmFetch.json(npa(pkgs[0]).escapedName, opts)
  const versions = Object.keys(json.versions)
  if (!versions.length)
    return pkgs
  else
    return versions.map(v => `${pkgs[0]}@${v}`)
}

async function unpublish (args) {
  if (args.length > 1)
    throw new Error(usage)

  const spec = args.length && npa(args[0])
  const opts = npm.flatOptions
  const { force, silent, loglevel } = opts
  let res
  let pkgName
  let pkgVersion

  log.silly('unpublish', 'args[0]', args[0])
  log.silly('unpublish', 'spec', spec)

  if (!spec.rawSpec && !force) {
    throw new Error(
      'Refusing to delete entire project.\n' +
      'Run with --force to do this.\n' +
      usage
    )
  }

  if (!spec || path.resolve(spec.name) === npm.localPrefix) {
    // if there's a package.json in the current folder, then
    // read the package name and version out of that.
    const pkgJson = path.join(npm.localPrefix, 'package.json')
    let manifest
    try {
      manifest = await readJson(pkgJson)
    } catch (err) {
      if (err && err.code !== 'ENOENT' && err.code !== 'ENOTDIR')
        throw err
      else
        throw new Error(`Usage: ${usage}`)
    }

    log.verbose('unpublish', manifest)

    const { name, version, publishConfig } = manifest
    const pkgJsonSpec = npa.resolve(name, version)
    const optsWithPub = { ...opts, publishConfig }
    res = await otplease(opts, opts => libunpub(pkgJsonSpec, optsWithPub))
    pkgName = name
    pkgVersion = version ? `@${version}` : ''
  } else {
    res = await otplease(opts, opts => libunpub(spec, opts))
    pkgName = spec.name
    pkgVersion = spec.type === 'version' ? `@${spec.rawSpec}` : ''
  }

  if (!silent && loglevel !== 'silent')
    output(`- ${pkgName}${pkgVersion}`)

  return res
}

module.exports = Object.assign(cmd, { completion, usage })
