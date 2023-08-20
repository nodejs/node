
const promzard = require('promzard')
const path = require('path')
const fs = require('fs/promises')
const semver = require('semver')
const read = require('read')
const util = require('util')
const rpj = require('read-package-json')

const def = require.resolve('./default-input.js')

// to validate the data object at the end as a worthwhile package
// and assign default values for things.
const _extraSet = rpj.extraSet
const _rpj = util.promisify(rpj)
const _rpjExtras = util.promisify(rpj.extras)
const readPkgJson = async (file, pkg) => {
  // only do a few of these. no need for mans or contributors if they're in the files
  rpj.extraSet = _extraSet.filter(f => f.name !== 'authors' && f.name !== 'mans')
  const p = pkg ? _rpjExtras(file, pkg) : _rpj(file)
  return p.catch(() => ({})).finally(() => rpj.extraSet = _extraSet)
}

const isYes = (c) => !!(c.get('yes') || c.get('y') || c.get('force') || c.get('f'))

const getConfig = (c = {}) => {
  // accept either a plain-jane object, or a config object with a "get" method.
  if (typeof c.get !== 'function') {
    const data = c
    return {
      get: (k) => data[k],
      toJSON: () => data,
    }
  }
  return c
}

const stringifyPerson = (p) => {
  if (typeof p === 'string') {
    return p
  }
  const { name = '', url, web, email, mail } = p
  const u = url || web
  const e = email || mail
  return `${name}${e ? ` <${e}>` : ''}${u ? ` (${u})` : ''}`
}

async function init (dir, input = def, c = {}) {
  const config = getConfig(c)
  const yes = isYes(config)
  const packageFile = path.resolve(dir, 'package.json')

  const pkg = await readPkgJson(packageFile)

  if (!semver.valid(pkg.version)) {
    delete pkg.version
  }

  // make sure that the input is valid. if not, use the default
  const pzData = await promzard(path.resolve(input), {
    yes,
    config,
    filename: packageFile,
    dirname: path.dirname(packageFile),
    basename: path.basename(path.dirname(packageFile)),
    package: pkg,
  }, { backupFile: def })

  for (const [k, v] of Object.entries(pzData)) {
    if (v != null) {
      pkg[k] = v
    }
  }

  const pkgExtras = await readPkgJson(packageFile, pkg)

  // turn the objects into somewhat more humane strings.
  if (pkgExtras.author) {
    pkgExtras.author = stringifyPerson(pkgExtras.author)
  }

  for (const set of ['maintainers', 'contributors']) {
    if (Array.isArray(pkgExtras[set])) {
      pkgExtras[set] = pkgExtras[set].map(stringifyPerson)
    }
  }

  // no need for the readme now.
  delete pkgExtras.readme
  delete pkgExtras.readmeFilename

  // really don't want to have this lying around in the file
  delete pkgExtras._id

  // ditto
  delete pkgExtras.gitHead

  // if the repo is empty, remove it.
  if (!pkgExtras.repository) {
    delete pkgExtras.repository
  }

  // readJson filters out empty descriptions, but init-package-json
  // traditionally leaves them alone
  if (!pkgExtras.description) {
    pkgExtras.description = pzData.description
  }

  // optionalDependencies don't need to be repeated in two places
  if (pkgExtras.dependencies) {
    if (pkgExtras.optionalDependencies) {
      for (const name of Object.keys(pkgExtras.optionalDependencies)) {
        delete pkgExtras.dependencies[name]
      }
    }
    if (Object.keys(pkgExtras.dependencies).length === 0) {
      delete pkgExtras.dependencies
    }
  }

  const stringified = JSON.stringify(pkgExtras, null, 2) + '\n'
  const msg = util.format('%s:\n\n%s\n', packageFile, stringified)
  const write = () => fs.writeFile(packageFile, stringified, 'utf8')

  if (yes) {
    await write()
    if (!config.get('silent')) {
      console.log(`Wrote to ${msg}`)
    }
    return pkgExtras
  }

  console.log(`About to write to ${msg}`)
  const ok = await read({ prompt: 'Is this OK? ', default: 'yes' })
  if (!ok || !ok.toLowerCase().startsWith('y')) {
    console.log('Aborted.')
    return
  }

  await write()
  return pkgExtras
}

module.exports = init
module.exports.yes = isYes
