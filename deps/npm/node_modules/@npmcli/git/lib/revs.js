const pinflight = require('promise-inflight')
const spawn = require('./spawn.js')
const LRU = require('lru-cache')

const revsCache = new LRU({
  max: 100,
  ttl: 5 * 60 * 1000,
})

const linesToRevs = require('./lines-to-revs.js')

module.exports = async (repo, opts = {}) => {
  if (!opts.noGitRevCache) {
    const cached = revsCache.get(repo)
    if (cached) {
      return cached
    }
  }

  return pinflight(`ls-remote:${repo}`, () =>
    spawn(['ls-remote', repo], opts)
      .then(({ stdout }) => linesToRevs(stdout.trim().split('\n')))
      .then(revs => {
        revsCache.set(repo, revs)
        return revs
      })
  )
}
