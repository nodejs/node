const fs = require('./fs.js')
const getOptions = require('./common/get-options.js')
const withOwner = require('./with-owner.js')

const writeFile = async (file, data, opts) => {
  const options = getOptions(opts, {
    copy: ['encoding', 'mode', 'flag', 'signal'],
    wrap: 'encoding',
  })

  return withOwner(file, () => fs.writeFile(file, data, options), opts)
}

module.exports = writeFile
