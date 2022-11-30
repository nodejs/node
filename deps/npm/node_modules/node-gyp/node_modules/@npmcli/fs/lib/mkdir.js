const fs = require('./fs.js')
const getOptions = require('./common/get-options.js')
const withOwner = require('./with-owner.js')

// extends mkdir with the ability to specify an owner of the new dir
const mkdir = async (path, opts) => {
  const options = getOptions(opts, {
    copy: ['mode', 'recursive'],
    wrap: 'mode',
  })

  return withOwner(
    path,
    () => fs.mkdir(path, options),
    opts
  )
}

module.exports = mkdir
