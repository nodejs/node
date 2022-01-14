const { resolve } = require('path')
const { promisify } = require('util')
const stat = promisify(require('fs').stat)
const walkUp = require('walk-up-path')

const fileExists = (file) => stat(file)
  .then((stat) => stat.isFile())
  .catch(() => false)

const localFileExists = async (dir, binName, root = '/') => {
  root = resolve(root).toLowerCase()

  for (const path of walkUp(resolve(dir))) {
    const binDir = resolve(path, 'node_modules', '.bin')

    if (await fileExists(resolve(binDir, binName))) {
      return binDir
    }

    if (path.toLowerCase() === root) {
      return false
    }
  }

  return false
}

module.exports = {
  fileExists,
  localFileExists,
}
