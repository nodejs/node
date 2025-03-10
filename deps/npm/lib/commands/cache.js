const fs = require('node:fs/promises')
const { join } = require('node:path')
const cacache = require('cacache')
const pacote = require('pacote')
const semver = require('semver')
const npa = require('npm-package-arg')
const jsonParse = require('json-parse-even-better-errors')
const localeCompare = require('@isaacs/string-locale-compare')('en')
const { log, output } = require('proc-log')
const PkgJson = require('@npmcli/package-json')
const abbrev = require('abbrev')
const BaseCommand = require('../base-cmd.js')

const searchCachePackage = async (path, parsed, cacheKeys) => {
  const searchMFH = new RegExp(`^make-fetch-happen:request-cache:.*(?<!/[@a-zA-Z]+)/${parsed.name}/-/(${parsed.name}[^/]+.tgz)$`)
  const searchPack = new RegExp(`^make-fetch-happen:request-cache:.*/${parsed.escapedName}$`)
  const results = new Set()
  cacheKeys = new Set(cacheKeys)
  for (const key of cacheKeys) {
    // match on the public key registry url format
    if (searchMFH.test(key)) {
      // extract the version from the filename
      const filename = key.match(searchMFH)[1]
      const noExt = filename.slice(0, -4)
      const noScope = `${parsed.name.split('/').pop()}-`
      const ver = noExt.slice(noScope.length)
      if (semver.satisfies(ver, parsed.rawSpec)) {
        results.add(key)
      }
      continue
    }
    // is this key a packument?
    if (!searchPack.test(key)) {
      continue
    }

    results.add(key)
    let packument, details
    try {
      details = await cacache.get(path, key)
      packument = jsonParse(details.data)
    } catch {
      // if we couldn't parse the packument, abort
      continue
    }
    if (!packument.versions || typeof packument.versions !== 'object') {
      continue
    }

    // assuming this is a packument
    for (const ver of Object.keys(packument.versions)) {
      if (semver.satisfies(ver, parsed.rawSpec)) {
        if (packument.versions[ver].dist &&
          typeof packument.versions[ver].dist === 'object' &&
          packument.versions[ver].dist.tarball !== undefined &&
          cacheKeys.has(`make-fetch-happen:request-cache:${packument.versions[ver].dist.tarball}`)
        ) {
          results.add(`make-fetch-happen:request-cache:${packument.versions[ver].dist.tarball}`)
        }
      }
    }
  }
  return results
}

class Cache extends BaseCommand {
  static description = 'Manipulates packages and npx cache'
  static name = 'cache'
  static params = ['cache']
  static usage = [
    'add <package-spec>',
    'clean [<key>]',
    'ls [<name>@<version>]',
    'verify',
    'npx ls',
    'npx rm [<key>...]',
    'npx info <key>...',
  ]

  static async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2) {
      return ['add', 'clean', 'verify', 'ls', 'npx']
    }

    // TODO - eventually...
    switch (argv[2]) {
      case 'verify':
      case 'clean':
      case 'add':
      case 'ls':
        return []
    }
  }

  async exec (args) {
    const cmd = args.shift()
    switch (cmd) {
      case 'rm': case 'clear': case 'clean':
        return await this.clean(args)
      case 'add':
        return await this.add(args)
      case 'verify': case 'check':
        return await this.verify()
      case 'ls':
        return await this.ls(args)
      case 'npx':
        return await this.npx(args)
      default:
        throw this.usageError()
    }
  }

  // npm cache npx
  async npx ([cmd, ...keys]) {
    switch (cmd) {
      case 'ls':
        return await this.npxLs(keys)
      case 'rm':
        return await this.npxRm(keys)
      case 'info':
        return await this.npxInfo(keys)
      default:
        throw this.usageError()
    }
  }

  // npm cache clean [spec]*
  async clean (args) {
    // this is a derived value
    const cachePath = this.npm.flatOptions.cache
    if (args.length === 0) {
      if (!this.npm.config.get('force')) {
        throw new Error(`As of npm@5, the npm cache self-heals from corruption issues
  by treating integrity mismatches as cache misses.  As a result,
  data extracted from the cache is guaranteed to be valid.  If you
  want to make sure everything is consistent, use \`npm cache verify\`
  instead.  Deleting the cache can only make npm go slower, and is
  not likely to correct any problems you may be encountering!

  On the other hand, if you're debugging an issue with the installer,
  or race conditions that depend on the timing of writing to an empty
  cache, you can use \`npm install --cache /tmp/empty-cache\` to use a
  temporary cache instead of nuking the actual one.

  If you're sure you want to delete the entire cache, rerun this command
  with --force.`)
      }
      return fs.rm(cachePath, { recursive: true, force: true })
    }
    for (const key of args) {
      let entry
      try {
        entry = await cacache.get(cachePath, key)
      } catch {
        log.warn('cache', `Not Found: ${key}`)
        break
      }
      output.standard(`Deleted: ${key}`)
      await cacache.rm.entry(cachePath, key)
      // XXX this could leave other entries without content!
      await cacache.rm.content(cachePath, entry.integrity)
    }
  }

  // npm cache add <tarball-url>...
  // npm cache add <pkg> <ver>...
  // npm cache add <tarball>...
  // npm cache add <folder>...
  async add (args) {
    log.silly('cache add', 'args', args)
    if (args.length === 0) {
      throw this.usageError('First argument to `add` is required')
    }

    await Promise.all(args.map(async spec => {
      log.silly('cache add', 'spec', spec)
      // we ask pacote for the thing, and then just throw the data
      // away so that it tee-pipes it into the cache like it does
      // for a normal request.
      await pacote.tarball.stream(spec, stream => {
        stream.resume()
        return stream.promise()
      }, { ...this.npm.flatOptions })

      await pacote.manifest(spec, {
        ...this.npm.flatOptions,
        fullMetadata: true,
      })
    }))
  }

  async verify () {
    // this is a derived value
    const cachePath = this.npm.flatOptions.cache
    const prefix = cachePath.indexOf(process.env.HOME) === 0
      ? `~${cachePath.slice(process.env.HOME.length)}`
      : cachePath
    const stats = await cacache.verify(cachePath)
    output.standard(`Cache verified and compressed (${prefix})`)
    output.standard(`Content verified: ${stats.verifiedContent} (${stats.keptSize} bytes)`)
    if (stats.badContentCount) {
      output.standard(`Corrupted content removed: ${stats.badContentCount}`)
    }
    if (stats.reclaimedCount) {
      output.standard(`Content garbage-collected: ${stats.reclaimedCount} (${stats.reclaimedSize} bytes)`)
    }
    if (stats.missingContent) {
      output.standard(`Missing content: ${stats.missingContent}`)
    }
    output.standard(`Index entries: ${stats.totalEntries}`)
    output.standard(`Finished in ${stats.runTime.total / 1000}s`)
  }

  // npm cache ls [<spec> ...]
  async ls (specs) {
    // This is a derived value
    const { cache: cachePath } = this.npm.flatOptions
    const cacheKeys = Object.keys(await cacache.ls(cachePath))
    if (specs.length > 0) {
      // get results for each package spec specified
      const results = new Set()
      for (const spec of specs) {
        const parsed = npa(spec)
        if (parsed.rawSpec !== '' && parsed.type === 'tag') {
          throw this.usageError('Cannot list cache keys for a tagged package.')
        }
        const keySet = await searchCachePackage(cachePath, parsed, cacheKeys)
        for (const key of keySet) {
          results.add(key)
        }
      }
      [...results].sort(localeCompare).forEach(key => output.standard(key))
      return
    }
    cacheKeys.sort(localeCompare).forEach(key => output.standard(key))
  }

  async #npxCache (keys = []) {
    // This is a derived value
    const { npxCache } = this.npm.flatOptions
    let dirs
    try {
      dirs = await fs.readdir(npxCache, { encoding: 'utf-8' })
    } catch {
      output.standard('npx cache does not exist')
      return
    }
    const cache = {}
    const { default: pMap } = await import('p-map')
    await pMap(dirs, async e => {
      const pkgPath = join(npxCache, e)
      cache[e] = {
        hash: e,
        path: pkgPath,
        valid: false,
      }
      try {
        const pkgJson = await PkgJson.load(pkgPath)
        cache[e].package = pkgJson.content
        cache[e].valid = true
      } catch {
        // Defaults to not valid already
      }
    }, { concurrency: 20 })
    if (!keys.length) {
      return cache
    }
    const result = {}
    const abbrevs = abbrev(Object.keys(cache))
    for (const key of keys) {
      if (!abbrevs[key]) {
        throw this.usageError(`Invalid npx key ${key}`)
      }
      result[abbrevs[key]] = cache[abbrevs[key]]
    }
    return result
  }

  async npxLs () {
    const cache = await this.#npxCache()
    for (const key in cache) {
      const { hash, valid, package: pkg } = cache[key]
      let result = `${hash}:`
      if (!valid) {
        result = `${result} (empty/invalid)`
      } else if (pkg?._npx) {
        result = `${result} ${pkg._npx.packages.join(', ')}`
      } else {
        result = `${result} (unknown)`
      }
      output.standard(result)
    }
  }

  async npxRm (keys) {
    if (!keys.length) {
      if (!this.npm.config.get('force')) {
        throw this.usageError('Please use --force to remove entire npx cache')
      }
      const { npxCache } = this.npm.flatOptions
      if (!this.npm.config.get('dry-run')) {
        return fs.rm(npxCache, { recursive: true, force: true })
      }
    }

    const cache = await this.#npxCache(keys)
    for (const key in cache) {
      const { path: cachePath } = cache[key]
      output.standard(`Removing npx key at ${cachePath}`)
      if (!this.npm.config.get('dry-run')) {
        await fs.rm(cachePath, { recursive: true })
      }
    }
  }

  async npxInfo (keys) {
    const chalk = this.npm.chalk
    if (!keys.length) {
      throw this.usageError()
    }
    const cache = await this.#npxCache(keys)
    const Arborist = require('@npmcli/arborist')
    for (const key in cache) {
      const { hash, path, package: pkg } = cache[key]
      let valid = cache[key].valid
      const results = []
      try {
        if (valid) {
          const arb = new Arborist({ path })
          const tree = await arb.loadVirtual()
          if (pkg._npx) {
            results.push('packages:')
            for (const p of pkg._npx.packages) {
              const parsed = npa(p)
              if (parsed.type === 'directory') {
                // in the tree the spec is relative, even if the dependency spec is absolute, so we can't find it by name or spec.
                results.push(`- ${chalk.cyan(p)}`)
              } else {
                results.push(`- ${chalk.cyan(p)} (${chalk.blue(tree.children.get(parsed.name).pkgid)})`)
              }
            }
          } else {
            results.push('packages: (unknown)')
            results.push(`dependencies:`)
            for (const dep in pkg.dependencies) {
              const child = tree.children.get(dep)
              if (child.isLink) {
                results.push(`- ${chalk.cyan(child.realpath)}`)
              } else {
                results.push(`- ${chalk.cyan(child.pkgid)}`)
              }
            }
          }
        }
      } catch (ex) {
        valid = false
      }
      const v = valid ? chalk.green('valid') : chalk.red('invalid')
      output.standard(`${v} npx cache entry with key ${chalk.blue(hash)}`)
      output.standard(`location: ${chalk.blue(path)}`)
      if (valid) {
        output.standard(results.join('\n'))
      }
      output.standard('')
    }
  }
}

module.exports = Cache
