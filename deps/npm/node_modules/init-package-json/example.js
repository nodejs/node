var init = require('./init-package-json.js')
var path = require('path')
var initFile = path.resolve(process.env.HOME, '.npm-init')
var dir = process.cwd()

var npm = require('npm')
npm.load(function (er, npm) {
  if (er) throw er
  init(dir, initFile, npm.config.get(), function (er, data) {
    if (er) throw er
    console.log('written successfully')
  })
})

