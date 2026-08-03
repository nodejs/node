const spawn = require('./spawn.js')
const { LRUCache } = require('lru-cache')
const linesToRevs = require('./lines-to-revs.js')

const revsCache = new LRUCache({
  max: 100,
  ttl: 5 * 60 * 1000,
})

module.exports = async (repo, opts = {}) => {
  if (!opts.noGitRevCache) {
    const cached = revsCache.get(repo)
    if (cached) {
      return cached
    }
  }

  const { stdout } = await spawn(['ls-remote', repo], opts)
  const revs = linesToRevs(stdout.trim().split('\n'))
  revsCache.set(repo, revs)
  return revs
}
