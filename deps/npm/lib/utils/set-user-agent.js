// Expand placeholder values in user-agent
const ciDetect = require('@npmcli/ci-detect')
const { version } = require('../../package.json')
module.exports = config => {
  const ciName = ciDetect()
  const ci = ciName ? `ci/${ciName}` : ''
  const ua = (config.get('user-agent') || '')
    .replace(/\{node-version\}/gi, config.get('node-version'))
    .replace(/\{npm-version\}/gi, version)
    .replace(/\{platform\}/gi, process.platform)
    .replace(/\{arch\}/gi, process.arch)
    .replace(/\{ci\}/gi, ci)

  config.set('user-agent', ua.trim())
}
