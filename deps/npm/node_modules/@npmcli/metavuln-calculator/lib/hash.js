const {createHash} = require('crypto')

module.exports = ({name, source}) => createHash('sha512')
  .update(JSON.stringify([name, source]))
  .digest('base64')
