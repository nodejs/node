const util = require('util')
const fs = require('fs')
const { stat } = fs.promises || { stat: util.promisify(fs.stat) }
const { resolve } = require('path')
module.exports = async path => {
  try {
    const st = await stat(resolve(path, 'server.js'))
    return st.isFile()
  } catch (er) {
    return false
  }
}
