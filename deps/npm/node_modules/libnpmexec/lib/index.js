'use strict'

const { promisify } = require('util')

const Arborist = require('@npmcli/arborist')
const ciDetect = require('@npmcli/ci-detect')
const crypto = require('crypto')
const log = require('proc-log')
const mkdirp = require('mkdirp-infer-owner')
const npa = require('npm-package-arg')
const npmlog = require('npmlog')
const pacote = require('pacote')
const read = promisify(require('read'))
const semver = require('semver')

const { fileExists, localFileExists } = require('./file-exists.js')
const getBinFromManifest = require('./get-bin-from-manifest.js')
const noTTY = require('./no-tty.js')
const runScript = require('./run-script.js')
const isWindows = require('./is-windows.js')

const { dirname, resolve } = require('path')

const binPaths = []

// when checking the local tree we look up manifests, cache those results by
// spec.raw so we don't have to fetch again when we check npxCache
const manifests = new Map()

const getManifest = async (spec, flatOptions) => {
  if (!manifests.has(spec.raw)) {
    const manifest = await pacote.manifest(spec, { ...flatOptions, preferOnline: true })
    manifests.set(spec.raw, manifest)
  }
  return manifests.get(spec.raw)
}

// Returns the required manifest if the spec is missing from the tree
// Returns the found node if it is in the tree
const missingFromTree = async ({ spec, tree, flatOptions }) => {
  if (spec.registry && (spec.rawSpec === '' || spec.type !== 'tag')) {
    // registry spec that is not a specific tag.
    const nodesBySpec = tree.inventory.query('packageName', spec.name)
    for (const node of nodesBySpec) {
      if (spec.type === 'tag') {
        // package requested by name only
        return { node }
      } else if (spec.type === 'version') {
        // package requested by specific version
        if (node.pkgid === spec.raw) {
          return { node }
        }
      } else {
        // package requested by version range, only remaining registry type
        if (semver.satisfies(node.package.version, spec.rawSpec)) {
          return { node }
        }
      }
    }
    const manifest = await getManifest(spec, flatOptions)
    return { manifest }
  } else {
    // non-registry spec, or a specific tag.  Look up manifest and check
    // resolved to see if it's in the tree.
    const manifest = await getManifest(spec, flatOptions)
    const nodesByManifest = tree.inventory.query('packageName', manifest.name)
    for (const node of nodesByManifest) {
      if (node.package.resolved === manifest._resolved) {
        // we have a package by the same name and the same resolved destination, nothing to add.
        return { node }
      }
    }
    return { manifest }
  }
}

const exec = async (opts) => {
  const {
    args = [],
    call = '',
    color = false,
    localBin = resolve('./node_modules/.bin'),
    locationMsg = undefined,
    globalBin = '',
    globalPath,
    output,
    // dereference values because we manipulate it later
    packages: [...packages] = [],
    path = '.',
    runPath = '.',
    scriptShell = isWindows ? process.env.ComSpec || 'cmd' : 'sh',
    yes = undefined,
    ...flatOptions
  } = opts

  const run = () => runScript({
    args,
    call,
    color,
    flatOptions,
    locationMsg,
    output,
    path,
    binPaths,
    runPath,
    scriptShell,
  })

  // interactive mode
  if (!call && !args.length && !packages.length) {
    return run()
  }

  const needPackageCommandSwap = (args.length > 0) && (packages.length === 0)
  // If they asked for a command w/o specifying a package, see if there is a
  // bin that directly matches that name either globally or in the local tree.
  if (needPackageCommandSwap) {
    const dir = dirname(dirname(localBin))
    const localBinPath = await localFileExists(dir, args[0], '/')
    if (localBinPath) {
      binPaths.push(localBinPath)
      return await run()
    } else if (globalPath && await fileExists(`${globalBin}/${args[0]}`)) {
      binPaths.push(globalBin)
      return await run()
    }

    // We swap out args[0] with the bin from the manifest later
    packages.push(args[0])
  }

  const localArb = new Arborist({ ...flatOptions, path })
  const localTree = await localArb.loadActual()

  // Find anything that isn't installed locally
  const needInstall = []
  let commandManifest
  await Promise.all(packages.map(async (pkg, i) => {
    const spec = npa(pkg, path)
    const { manifest, node } = await missingFromTree({ spec, tree: localTree, flatOptions })
    if (manifest) {
      // Package does not exist in the local tree
      needInstall.push({ spec, manifest })
      if (i === 0) {
        commandManifest = manifest
      }
    } else if (i === 0) {
      // The node.package has enough to look up the bin
      commandManifest = node.package
    }
  }))

  if (needPackageCommandSwap) {
    const spec = npa(args[0])

    args[0] = getBinFromManifest(commandManifest)

    if (needInstall.length > 0 && globalPath) {
      // See if the package is installed globally, and run the translated bin
      const globalArb = new Arborist({ ...flatOptions, path: globalPath, global: true })
      const globalTree = await globalArb.loadActual()
      const { manifest: globalManifest } =
        await missingFromTree({ spec, tree: globalTree, flatOptions })
      if (!globalManifest && await fileExists(`${globalBin}/${args[0]}`)) {
        binPaths.push(globalBin)
        return await run()
      }
    }
  }

  const add = []
  if (needInstall.length > 0) {
    // Install things to the npx cache, if needed
    const { npxCache } = flatOptions
    if (!npxCache) {
      throw new Error('Must provide a valid npxCache path')
    }
    const hash = crypto.createHash('sha512')
      .update(packages.sort((a, b) => a.localeCompare(b, 'en')).join('\n'))
      .digest('hex')
      .slice(0, 16)
    const installDir = resolve(npxCache, hash)
    await mkdirp(installDir)
    const npxArb = new Arborist({
      ...flatOptions,
      path: installDir,
    })
    const npxTree = await npxArb.loadActual()
    await Promise.all(needInstall.map(async ({ spec }) => {
      const { manifest } = await missingFromTree({ spec, tree: npxTree, flatOptions })
      if (manifest) {
        // Manifest is not in npxCache, we need to install it there
        if (!spec.registry) {
          add.push(manifest._from)
        } else {
          add.push(manifest._id)
        }
      }
    }))

    if (add.length) {
      if (!yes) {
        // set -n to always say no
        if (yes === false) {
          throw new Error('canceled')
        }

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
          npmlog.clearProgress()
          const confirm = await read({ prompt, default: 'y' })
          if (confirm.trim().toLowerCase().charAt(0) !== 'y') {
            throw new Error('canceled')
          }
        }
      }
      await npxArb.reify({
        ...flatOptions,
        add,
      })
    }
    binPaths.push(resolve(installDir, 'node_modules/.bin'))
  }

  return await run()
}

module.exports = exec
