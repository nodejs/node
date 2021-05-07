const full = process.env.npm_lifecycle_event === 'check-coverage'
const coverageMap = (filename) => {
  if (full && /load-all.js$/.test(filename)) {
    const glob = require('glob')
    const { resolve, relative } = require('path')
    const dir = resolve(__dirname, '../lib')
    return glob.sync(`${dir}/**/*.js`)
      .map(f => relative(process.cwd(), f))
  }
  if (/windows-shims\.js$/.test(filename)) {
    // this one doesn't provide any coverage nyc can track
    return []
  }
  if (/^test\/(lib|bin)\//.test(filename))
    return filename.replace(/^test\//, '')
  return []
}

module.exports = coverageMap
