const fs = require('fs')
const util = require('util')

const stat = util.promisify(fs.stat)

const fileExists = (file) => stat(file)
  .then((stat) => stat.isFile())
  .catch(() => false)

module.exports = fileExists
