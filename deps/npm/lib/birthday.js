const npm = require('./npm.js')
module.exports = (_, cb) => {
  Object.defineProperty(npm, 'flatOptions', {
    value: {
      ...npm.flatOptions,
      package: ['@npmcli/npm-birthday'],
      yes: true,
    },
  })
  return npm.commands.exec(['npm-birthday'], cb)
}
