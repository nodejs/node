var init = require('../init-package-json.js')
var path = require('path')
var dir = process.cwd()
var npm = require('npm')

npm.load(function (er, npm) {
  if (er) throw er
  init(dir, npm.config.get('init-module'), npm.config, function (er, data) {
    if (er) throw er
    console.log('written successfully')
  })
})

