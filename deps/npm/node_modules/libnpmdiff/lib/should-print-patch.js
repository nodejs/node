const { basename, extname } = require('path')

const binaryExtensions = require('binary-extensions')

// we should try to print patches as long as the
// extension is not identified as binary files
const shouldPrintPatch = (path, opts = {}) => {
  if (opts.diffText) {
    return true
  }

  const filename = basename(path)
  const extension = (
    filename.startsWith('.')
      ? filename
      : extname(filename)
  ).substr(1)

  return !binaryExtensions.includes(extension)
}

module.exports = shouldPrintPatch
