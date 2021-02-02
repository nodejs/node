const npm = require('./npm.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')
const usage = usageUtil('exec',
  'Run a command from a local or remote npm package.\n\n' +

  'npm exec -- <pkg>[@<version>] [args...]\n' +
  'npm exec --package=<pkg>[@<version>] -- <cmd> [args...]\n' +
  'npm exec -c \'<cmd> [args...]\'\n' +
  'npm exec --package=foo -c \'<cmd> [args...]\'\n' +
  '\n' +
  'npx <pkg>[@<specifier>] [args...]\n' +
  'npx -p <pkg>[@<specifier>] <cmd> [args...]\n' +
  'npx -c \'<cmd> [args...]\'\n' +
  'npx -p <pkg>[@<specifier>] -c \'<cmd> [args...]\'' +
  '\n' +
  'Run without --call or positional args to open interactive subshell\n',

  '\n--package=<pkg> (may be specified multiple times)\n' +
  '-p is a shorthand for --package only when using npx executable\n' +
  '-c <cmd> --call=<cmd> (may not be mixed with positional arguments)'
)

const completion = require('./utils/completion/installed-shallow.js')

const { promisify } = require('util')
const read = promisify(require('read'))

// it's like this:
//
// npm x pkg@version <-- runs the bin named "pkg" or the only bin if only 1
//
// { name: 'pkg', bin: { pkg: 'pkg.js', foo: 'foo.js' }} <-- run pkg
// { name: 'pkg', bin: { foo: 'foo.js' }} <-- run foo?
//
// npm x -p pkg@version -- foo
//
// npm x -p pkg@version -- foo --registry=/dev/null
//
// const pkg = npm.flatOptions.package || getPackageFrom(args[0])
// const cmd = getCommand(pkg, args[0])
// --> npm x -c 'cmd ...args.slice(1)'
//
// we've resolved cmd and args, and escaped them properly, and installed the
// relevant packages.
//
// Add the ${npx install prefix}/node_modules/.bin to PATH
//
// pkg = readPackageJson('./package.json')
// pkg.scripts.___npx = ${the -c arg}
// runScript({ pkg, event: 'npx', ... })
// process.env.npm_lifecycle_event = 'npx'

const mkdirp = require('mkdirp-infer-owner')
const readPackageJson = require('read-package-json-fast')
const Arborist = require('@npmcli/arborist')
const runScript = require('@npmcli/run-script')
const { resolve, delimiter } = require('path')
const ciDetect = require('@npmcli/ci-detect')
const crypto = require('crypto')
const pacote = require('pacote')
const npa = require('npm-package-arg')
const fileExists = require('./utils/file-exists.js')
const PATH = require('./utils/path.js')

const cmd = (args, cb) => exec(args).then(() => cb()).catch(cb)

const run = async ({ args, call, pathArr, shell }) => {
  // turn list of args into command string
  const script = call || args.shift() || shell

  // do the fakey runScript dance
  // still should work if no package.json in cwd
  const realPkg = await readPackageJson(`${npm.localPrefix}/package.json`)
    .catch(() => ({}))
  const pkg = {
    ...realPkg,
    scripts: {
      ...(realPkg.scripts || {}),
      npx: script,
    },
  }

  npm.log.disableProgress()
  try {
    if (script === shell) {
      if (process.stdin.isTTY) {
        if (ciDetect())
          return npm.log.warn('exec', 'Interactive mode disabled in CI environment')
        output(`\nEntering npm script environment\nType 'exit' or ^D when finished\n`)
      }
    }
    return await runScript({
      ...npm.flatOptions,
      pkg,
      banner: false,
      // we always run in cwd, not --prefix
      path: process.cwd(),
      stdioString: true,
      event: 'npx',
      args,
      env: {
        PATH: pathArr.join(delimiter),
      },
      stdio: 'inherit',
    })
  } finally {
    npm.log.enableProgress()
  }
}

const exec = async args => {
  const { package: packages, call, shell } = npm.flatOptions

  if (call && args.length)
    throw usage

  const pathArr = [...PATH]

  // nothing to maybe install, skip the arborist dance
  if (!call && !args.length && !packages.length) {
    return await run({
      args,
      call,
      shell,
      pathArr,
    })
  }

  const needPackageCommandSwap = args.length && !packages.length
  // if there's an argument and no package has been explicitly asked for
  // check the local and global bin paths for a binary named the same as
  // the argument and run it if it exists, otherwise fall through to
  // the behavior of treating the single argument as a package name
  if (needPackageCommandSwap) {
    let binExists = false
    if (await fileExists(`${npm.localBin}/${args[0]}`)) {
      pathArr.unshift(npm.localBin)
      binExists = true
    } else if (await fileExists(`${npm.globalBin}/${args[0]}`)) {
      pathArr.unshift(npm.globalBin)
      binExists = true
    }

    if (binExists) {
      return await run({
        args,
        call,
        pathArr,
        shell,
      })
    }

    packages.push(args[0])
  }

  // If we do `npm exec foo`, and have a `foo` locally, then we'll
  // always use that, so we don't really need to fetch the manifest.
  // So: run npa on each packages entry, and if it is a name with a
  // rawSpec==='', then try to readPackageJson at
  // node_modules/${name}/package.json, and only pacote fetch if
  // that fails.
  const manis = await Promise.all(packages.map(async p => {
    const spec = npa(p, npm.localPrefix)
    if (spec.type === 'tag' && spec.rawSpec === '') {
      // fall through to the pacote.manifest() approach
      try {
        const pj = resolve(npm.localPrefix, 'node_modules', spec.name)
        return await readPackageJson(pj)
      } catch (er) {}
    }
    // Force preferOnline to true so we are making sure to pull in the latest
    // This is especially useful if the user didn't give us a version, and
    // they expect to be running @latest
    return await pacote.manifest(p, {
      ...npm.flatOptions,
      preferOnline: true,
    })
  }))

  if (needPackageCommandSwap)
    args[0] = getBinFromManifest(manis[0])

  // figure out whether we need to install stuff, or if local is fine
  const localArb = new Arborist({
    ...npm.flatOptions,
    path: npm.localPrefix,
  })
  const tree = await localArb.loadActual()

  // do we have all the packages in manifest list?
  const needInstall = manis.some(mani => manifestMissing(tree, mani))

  if (needInstall) {
    const installDir = cacheInstallDir(packages)
    await mkdirp(installDir)
    const arb = new Arborist({ ...npm.flatOptions, path: installDir })
    const tree = await arb.loadActual()

    // at this point, we have to ensure that we get the exact same
    // version, because it's something that has only ever been installed
    // by npm exec in the cache install directory
    const add = manis.filter(mani => manifestMissing(tree, {
      ...mani,
      _from: `${mani.name}@${mani.version}`,
    }))
      .map(mani => mani._from)
      .sort((a, b) => a.localeCompare(b))

    // no need to install if already present
    if (add.length) {
      if (!npm.flatOptions.yes) {
        // set -n to always say no
        if (npm.flatOptions.yes === false)
          throw 'canceled'

        if (!process.stdin.isTTY || ciDetect()) {
          npm.log.warn('exec', `The following package${
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
            throw 'canceled'
        }
      }
      await arb.reify({ ...npm.flatOptions, add })
    }
    pathArr.unshift(resolve(installDir, 'node_modules/.bin'))
  }

  return await run({ args, call, pathArr, shell })
}

const manifestMissing = (tree, mani) => {
  // if the tree doesn't have a child by that name/version, return true
  // true means we need to install it
  const child = tree.children.get(mani.name)
  // if no child, we have to load it
  if (!child)
    return true

  // if no version/tag specified, allow whatever's there
  if (mani._from === `${mani.name}@`)
    return false

  // otherwise the version has to match what we WOULD get
  return child.version !== mani.version
}

const getBinFromManifest = mani => {
  // if we have a bin matching (unscoped portion of) packagename, use that
  // otherwise if there's 1 bin or all bin value is the same (alias), use that,
  // otherwise fail
  const bin = mani.bin || {}
  if (new Set(Object.values(bin)).size === 1)
    return Object.keys(bin)[0]

  // XXX probably a util to parse this better?
  const name = mani.name.replace(/^@[^/]+\//, '')
  if (bin[name])
    return name

  // XXX need better error message
  throw Object.assign(new Error('could not determine executable to run'), {
    pkgid: mani._id,
  })
}

// only packages not found in ${prefix}/node_modules
const cacheInstallDir = packages =>
  resolve(npm.config.get('cache'), '_npx', getHash(packages))

const getHash = packages =>
  crypto.createHash('sha512')
    .update(packages.sort((a, b) => a.localeCompare(b)).join('\n'))
    .digest('hex')
    .slice(0, 16)

module.exports = Object.assign(cmd, { completion, usage })
