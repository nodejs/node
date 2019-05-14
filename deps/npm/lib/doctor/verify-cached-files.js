'use strict'

const cacache = require('cacache')
const log = require('npmlog')

module.exports = verifyCachedFiles
function verifyCachedFiles (cache, cb) {
  log.info('verifyCachedFiles', `Verifying cache at ${cache}`)
  cacache.verify(cache).then((stats) => {
    log.info('verifyCachedFiles', `Verification complete. Stats: ${JSON.stringify(stats, 2)}`)
    if (stats.reclaimedCount || stats.badContentCount || stats.missingContent) {
      stats.badContentCount && log.warn('verifyCachedFiles', `Corrupted content removed: ${stats.badContentCount}`)
      stats.reclaimedCount && log.warn('verifyCachedFiles', `Content garbage-collected: ${stats.reclaimedCount} (${stats.reclaimedSize} bytes)`)
      stats.missingContent && log.warn('verifyCachedFiles', `Missing content: ${stats.missingContent}`)
      log.warn('verifyCachedFiles', 'Cache issues have been fixed')
    }
    return stats
  }).then((s) => cb(null, s), cb)
}
