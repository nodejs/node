const cacache = require('cacache')
const { promisify } = require('util')
const log = require('npmlog')
const pacote = require('pacote')
const path = require('path')
const rimraf = promisify(require('rimraf'))
const BaseCommand = require('./base-command.js')

class Cache extends BaseCommand {
  static get description () {
    return 'Manipulates packages cache'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'cache'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      'add <tarball file>',
      'add <folder>',
      'add <tarball url>',
      'add <git url>',
      'add <name>@<version>',
      'clean',
      'verify',
    ]
  }

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2)
      return ['add', 'clean', 'verify']

    // TODO - eventually...
    switch (argv[2]) {
      case 'verify':
      case 'clean':
      case 'add':
        return []
    }
  }

  exec (args, cb) {
    this.cache(args).then(() => cb()).catch(cb)
  }

  async cache (args) {
    const cmd = args.shift()
    switch (cmd) {
      case 'rm': case 'clear': case 'clean':
        return await this.clean(args)
      case 'add':
        return await this.add(args)
      case 'verify': case 'check':
        return await this.verify()
      default:
        throw Object.assign(new Error(this.usage), { code: 'EUSAGE' })
    }
  }

  // npm cache clean [pkg]*
  async clean (args) {
    if (args.length)
      throw new Error('npm cache clear does not accept arguments')

    const cachePath = path.join(this.npm.cache, '_cacache')
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

  // npm cache add <tarball-url>...
  // npm cache add <pkg> <ver>...
  // npm cache add <tarball>...
  // npm cache add <folder>...
  async add (args) {
    const usage = 'Usage:\n' +
      '    npm cache add <tarball-url>...\n' +
      '    npm cache add <pkg>@<ver>...\n' +
      '    npm cache add <tarball>...\n' +
      '    npm cache add <folder>...\n'
    log.silly('cache add', 'args', args)
    if (args.length === 0)
      throw Object.assign(new Error(usage), { code: 'EUSAGE' })

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
      ? `~${cache.substr(process.env.HOME.length)}`
      : cache
    const stats = await cacache.verify(cache)
    this.npm.output(`Cache verified and compressed (${prefix})`)
    this.npm.output(`Content verified: ${stats.verifiedContent} (${stats.keptSize} bytes)`)
    stats.badContentCount && this.npm.output(`Corrupted content removed: ${stats.badContentCount}`)
    stats.reclaimedCount && this.npm.output(`Content garbage-collected: ${stats.reclaimedCount} (${stats.reclaimedSize} bytes)`)
    stats.missingContent && this.npm.output(`Missing content: ${stats.missingContent}`)
    this.npm.output(`Index entries: ${stats.totalEntries}`)
    this.npm.output(`Finished in ${stats.runTime.total / 1000}s`)
  }
}

module.exports = Cache
