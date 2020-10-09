// Accepts a config object, returns a user-agent string
const getUserAgent = (config) => {
  const ciName = config.get('ci-name')
  return (config.get('user-agent') || '')
    .replace(/\{node-version\}/gi, config.get('node-version'))
    .replace(/\{npm-version\}/gi, config.get('npm-version'))
    .replace(/\{platform\}/gi, process.platform)
    .replace(/\{arch\}/gi, process.arch)
    .replace(/\{ci\}/gi, ciName ? `ci/${ciName}` : '')
    .trim()
}

module.exports = getUserAgent
