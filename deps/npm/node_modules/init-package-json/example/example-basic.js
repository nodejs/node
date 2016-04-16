var init = require('../init-package-json.js')
var dir = process.cwd()
var initFile = require.resolve('./init/basic-init.js')

init(dir, initFile, function (err, data) {
  if (!err) console.log('written successfully')
})
