const coverageMap = (filename) => {
  const { basename } = require('path')
  const testbase = basename(filename)
  if (filename === 'test/index.js') {
    return ['index.js']
  }
  if (testbase === 'load-all-commands.js') {
    const { cmdList } = require('../lib/utils/cmd-list.js')
    return cmdList.map(cmd => `lib/${cmd}.js`)
      .concat('lib/base-command.js')
  }
  if (/^test\/lib\/commands/.test(filename) || filename === 'test/lib/npm.js') {
    return [
      filename.replace(/^test\//, ''),
      'lib/base-command.js',
      'lib/exec/get-workspace-location-msg.js',
    ]
  }
  if (/^test\/(lib|bin)\//.test(filename)) {
    return filename.replace(/^test\//, '')
  }
  return []
}

module.exports = coverageMap
