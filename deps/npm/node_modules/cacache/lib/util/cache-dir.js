'use strict'

const fs = require('fs/promises')
const path = require('path')

const tagContent = `Signature: 8a477f597d28d172789f06886806bc55
# This file is a cache directory tag created by cacache.
# For information about cache directory tags, see https://bford.info/cachedir/
`

async function mkdir (cache) {
  await fs.mkdir(cache, { recursive: true, owner: 'inherit' })
  await writeTag(cache)
}

async function writeTag (cache) {
  try {
    await fs.writeFile(path.join(cache, 'CACHEDIR.TAG'), tagContent, { flag: 'wx' })
  } catch (err) {
    if (err.code !== 'EEXIST') {
      throw err
    }
  }
}

module.exports = {
  mkdir,
  tagContent,
}
