const { stat } = require('fs').promises
const { resolve } = require('path')
module.exports = async path => {
  try {
    const st = await stat(resolve(path, 'server.js'))
    return st.isFile()
  } catch (er) {
    return false
  }
}
