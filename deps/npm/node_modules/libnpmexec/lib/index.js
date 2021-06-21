const { delimiter, dirname, resolve } = require('path')
const { promisify } = require('util')
const read = promisify(require('read'))

const Arborist = require('@npmcli/arborist')
const ciDetect = require('@npmcli/ci-detect')
const logger = require('proc-log')
const mkdirp = require('mkdirp-infer-owner')
const npa = require('npm-package-arg')
const pacote = require('pacote')
const readPackageJson = require('read-package-json-fast')

const cacheInstallDir = require('./cache-install-dir.js')
const { fileExists, localFileExists } = require('./file-exists.js')
const getBinFromManifest = require('./get-bin-from-manifest.js')
const manifestMissing = require('./manifest-missing.js')
const noTTY = require('./no-tty.js')
const runScript = require('./run-script.js')
const isWindows = require('./is-windows.js')

/* istanbul ignore next */
const PATH = (
  process.env.PATH || process.env.Path || process.env.path
).split(delimiter)

const exec = async (opts) => {
  const {
    args = [],
    call = '',
    color = false,
    localBin = resolve('./node_modules/.bin'),
    locationMsg = undefined,
    globalBin = '',
    output,
    packages: _packages = [],
    path = '.',
    runPath = '.',
    scriptShell = isWindows ? process.env.ComSpec || 'cmd' : 'sh',
    yes = undefined,
    ...flatOptions
  } = opts
  const log = flatOptions.log || logger

  // dereferences values because we manipulate it later
  const packages = [..._packages]
  const pathArr = [...PATH]
  const _run = () => runScript({
    args,
    call,
    color,
    flatOptions,
    locationMsg,
    log,
    output,
    path,
    pathArr,
    runPath,
    scriptShell,
  })

  // nothing to maybe install, skip the arborist dance
  if (!call && !args.length && !packages.length)
    return await _run()

  const needPackageCommandSwap = args.length && !packages.length
  // if there's an argument and no package has been explicitly asked for
  // check the local and global bin paths for a binary named the same as
  // the argument and run it if it exists, otherwise fall through to
  // the behavior of treating the single argument as a package name
  if (needPackageCommandSwap) {
    let binExists = false
    const dir = dirname(dirname(localBin))
    const localBinPath = await localFileExists(dir, args[0])
    if (localBinPath) {
      pathArr.unshift(localBinPath)
      binExists = true
    } else if (await fileExists(`${globalBin}/${args[0]}`)) {
      pathArr.unshift(globalBin)
      binExists = true
    }

    if (binExists)
      return await _run()

    packages.push(args[0])
  }

  // If we do `npm exec foo`, and have a `foo` locally, then we'll
  // always use that, so we don't really need to fetch the manifest.
  // So: run npa on each packages entry, and if it is a name with a
  // rawSpec==='', then try to readPackageJson at
  // node_modules/${name}/package.json, and only pacote fetch if
  // that fails.
  const manis = await Promise.all(packages.map(async p => {
    const spec = npa(p, path)
    if (spec.type === 'tag' && spec.rawSpec === '') {
      // fall through to the pacote.manifest() approach
      try {
        const pj = resolve(path, 'node_modules', spec.name, 'package.json')
        return await readPackageJson(pj)
      } catch (er) {}
    }
    // Force preferOnline to true so we are making sure to pull in the latest
    // This is especially useful if the user didn't give us a version, and
    // they expect to be running @latest
    return await pacote.manifest(p, {
      ...flatOptions,
      preferOnline: true,
    })
  }))

  if (needPackageCommandSwap)
    args[0] = getBinFromManifest(manis[0])

  // figure out whether we need to install stuff, or if local is fine
  const localArb = new Arborist({
    ...flatOptions,
    path,
  })
  const tree = await localArb.loadActual()

  // do we have all the packages in manifest list?
  const needInstall =
    manis.some(manifest => manifestMissing({ tree, manifest }))

  if (needInstall) {
    const { npxCache } = flatOptions
    const installDir = cacheInstallDir({ npxCache, packages })
    await mkdirp(installDir)
    const arb = new Arborist({
      ...flatOptions,
      path: installDir,
    })
    const tree = await arb.loadActual()

    // at this point, we have to ensure that we get the exact same
    // version, because it's something that has only ever been installed
    // by npm exec in the cache install directory
    const add = manis.filter(mani => manifestMissing({
      tree,
      manifest: {
        ...mani,
        _from: `${mani.name}@${mani.version}`,
      },
    }))
      .map(mani => mani._from)
      .sort((a, b) => a.localeCompare(b, 'en'))

    // no need to install if already present
    if (add.length) {
      if (!yes) {
        // set -n to always say no
        if (yes === false)
          throw new Error('canceled')

        if (noTTY() || ciDetect()) {
          log.warn('exec', `The following package${
            add.length === 1 ? ' was' : 's were'
          } not found and will be installed: ${
            add.map((pkg) => pkg.replace(/@$/, '')).join(', ')
          }`)
        } else {
          const addList = add.map(a => `  ${a.replace(/@$/, '')}`)
            .join('\n') + '\n'
          const prompt = `Need to install the following packages:\n${
          addList
        }Ok to proceed? `
          const confirm = await read({ prompt, default: 'y' })
          if (confirm.trim().toLowerCase().charAt(0) !== 'y')
            throw new Error('canceled')
        }
      }
      await arb.reify({
        ...flatOptions,
        add,
      })
    }
    pathArr.unshift(resolve(installDir, 'node_modules/.bin'))
  }

  return await _run()
}

module.exports = exec
