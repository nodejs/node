const os = require('os')
const { resolve } = require('path')

module.exports = (fakePlatform = false) => {
  const temp = os.tmpdir()
  const uidOrPid = process.getuid ? process.getuid() : process.pid
  const home = os.homedir() || resolve(temp, 'npm-' + uidOrPid)
  const platform = fakePlatform || process.platform
  const cacheExtra = platform === 'win32' ? 'npm-cache' : '.npm'
  const cacheRoot = (platform === 'win32' && process.env.LOCALAPPDATA) || home
  return {
    cacache: resolve(cacheRoot, cacheExtra, '_cacache'),
    tufcache: resolve(cacheRoot, cacheExtra, '_tuf'),
  }
}
