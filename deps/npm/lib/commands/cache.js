const cacache = require('cacache')
const { promisify } = require('util')
const pacote = require('pacote')
const path = require('path')
const rimraf = promisify(require('rimraf'))
const semver = require('semver')
const BaseCommand = require('../base-command.js')
const npa = require('npm-package-arg')
const jsonParse = require('json-parse-even-better-errors')
const localeCompare = require('@isaacs/string-locale-compare')('en')
const log = require('../utils/log-shim')

const searchCachePackage = async (path, parsed, cacheKeys) => {
  /* eslint-disable-next-line max-len */
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
    } catch (_) {
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
  static description = 'Manipulates packages cache'
  static name = 'cache'
  static params = ['cache']
  static usage = [
    'add <tarball file>',
    'add <folder>',
    'add <tarball url>',
    'add <git url>',
    'add <name>@<version>',
    'clean [<key>]',
    'ls [<name>@<version>]',
    'verify',
  ]

  static ignoreImplicitWorkspace = true

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2) {
      return ['add', 'clean', 'verify', 'ls', 'delete']
    }

    // TODO - eventually...
    switch (argv[2]) {
      case 'verify':
      case 'clean':
      case 'add':
      case 'ls':
      case 'delete':
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
      default:
        throw this.usageError()
    }
  }

  // npm cache clean [pkg]*
  async clean (args) {
    const cachePath = path.join(this.npm.cache, '_cacache')
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
      return rimraf(cachePath)
    }
    for (const key of args) {
      let entry
      try {
        entry = await cacache.get(cachePath, key)
      } catch (err) {
        log.warn(`Not Found: ${key}`)
        break
      }
      this.npm.output(`Deleted: ${key}`)
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

    return Promise.all(args.map(spec => {
      log.silly('cache add', 'spec', spec)
      // we ask pacote for the thing, and then just throw the data
      // away so that it tee-pipes it into the cache like it does
      // for a normal request.
      return pacote.tarball.stream(spec, stream => {
        stream.resume()
        return stream.promise()
      }, this.npm.flatOptions)
    }))
  }

  async verify () {
    const cache = path.join(this.npm.cache, '_cacache')
    const prefix = cache.indexOf(process.env.HOME) === 0
      ? `~${cache.slice(process.env.HOME.length)}`
      : cache
    const stats = await cacache.verify(cache)
    this.npm.output(`Cache verified and compressed (${prefix})`)
    this.npm.output(`Content verified: ${stats.verifiedContent} (${stats.keptSize} bytes)`)
    if (stats.badContentCount) {
      this.npm.output(`Corrupted content removed: ${stats.badContentCount}`)
    }
    if (stats.reclaimedCount) {
      /* eslint-disable-next-line max-len */
      this.npm.output(`Content garbage-collected: ${stats.reclaimedCount} (${stats.reclaimedSize} bytes)`)
    }
    if (stats.missingContent) {
      this.npm.output(`Missing content: ${stats.missingContent}`)
    }
    this.npm.output(`Index entries: ${stats.totalEntries}`)
    this.npm.output(`Finished in ${stats.runTime.total / 1000}s`)
  }

  // npm cache ls [--package <spec> ...]
  async ls (specs) {
    const cachePath = path.join(this.npm.cache, '_cacache')
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
      [...results].sort(localeCompare).forEach(key => this.npm.output(key))
      return
    }
    cacheKeys.sort(localeCompare).forEach(key => this.npm.output(key))
  }
}

module.exports = Cache
