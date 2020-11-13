const cacache = require('cacache')
const { promisify } = require('util')
const log = require('npmlog')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const pacote = require('pacote')
const path = require('path')
const rimraf = promisify(require('rimraf'))

const usageUtil = require('./utils/usage.js')

const usage = usageUtil('cache',
  'npm cache add <tarball file>' +
  '\nnpm cache add <folder>' +
  '\nnpm cache add <tarball url>' +
  '\nnpm cache add <git url>' +
  '\nnpm cache add <name>@<version>' +
  '\nnpm cache clean' +
  '\nnpm cache verify'
)

const completion = (opts, cb) => {
  const argv = opts.conf.argv.remain
  if (argv.length === 2)
    return cb(null, ['add', 'clean', 'verify'])

  // TODO - eventually...
  switch (argv[2]) {
    case 'verify':
    case 'clean':
    case 'add':
      return cb(null, [])
  }
}

const cmd = (args, cb) => cache(args).then(() => cb()).catch(cb)

const cache = async (args) => {
  const cmd = args.shift()
  switch (cmd) {
    case 'rm': case 'clear': case 'clean':
      return await clean(args)
    case 'add':
      return await add(args)
    case 'verify': case 'check':
      return await verify()
    default:
      throw Object.assign(new Error(usage), { code: 'EUSAGE' })
  }
}

// npm cache clean [pkg]*
const clean = async (args) => {
  if (args.length)
    throw new Error('npm cache clear does not accept arguments')

  const cachePath = path.join(npm.cache, '_cacache')
  if (!npm.flatOptions.force) {
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

// npm cache add <tarball-url>
// npm cache add <pkg> <ver>
// npm cache add <tarball>
// npm cache add <folder>
const add = async (args) => {
  const usage = 'Usage:\n' +
    '    npm cache add <tarball-url>\n' +
    '    npm cache add <pkg>@<ver>\n' +
    '    npm cache add <tarball>\n' +
    '    npm cache add <folder>\n'
  log.silly('cache add', 'args', args)
  const spec = args[0] && args[0] +
    (args[1] === undefined || args[1] === null ? '' : `@${args[1]}`)

  if (!spec)
    throw Object.assign(new Error(usage), { code: 'EUSAGE' })

  log.silly('cache add', 'spec', spec)
  const opts = { ...npm.flatOptions }

  // we ask pacote for the thing, and then just throw the data
  // away so that it tee-pipes it into the cache like it does
  // for a normal request.
  await pacote.tarball.stream(spec, stream => {
    stream.resume()
    return stream.promise()
  }, opts)
}

const verify = async () => {
  const cache = path.join(npm.cache, '_cacache')
  const prefix = cache.indexOf(process.env.HOME) === 0
    ? `~${cache.substr(process.env.HOME.length)}`
    : cache
  const stats = await cacache.verify(cache)
  output(`Cache verified and compressed (${prefix})`)
  output(`Content verified: ${stats.verifiedContent} (${stats.keptSize} bytes)`)
  stats.badContentCount && output(`Corrupted content removed: ${stats.badContentCount}`)
  stats.reclaimedCount && output(`Content garbage-collected: ${stats.reclaimedCount} (${stats.reclaimedSize} bytes)`)
  stats.missingContent && output(`Missing content: ${stats.missingContent}`)
  output(`Index entries: ${stats.totalEntries}`)
  output(`Finished in ${stats.runTime.total / 1000}s`)
}

module.exports = Object.assign(cmd, { completion, usage })
