const { resolve } = require('path')
const fs = require('@npmcli/fs')
const walkUp = require('walk-up-path')

const fileExists = async (file) => {
  try {
    const res = await fs.stat(file)
    return res.isFile()
  } catch {
    return false
  }
}

const localFileExists = async (dir, binName, root) => {
  for (const path of walkUp(dir)) {
    const binDir = resolve(path, 'node_modules', '.bin')

    if (await fileExists(resolve(binDir, binName))) {
      return binDir
    }

    if (path.toLowerCase() === resolve(root).toLowerCase()) {
      return false
    }
  }

  return false
}

module.exports = {
  fileExists,
  localFileExists,
}
