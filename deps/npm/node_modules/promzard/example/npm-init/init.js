var PZ = require('../../promzard').PromZard
var path = require('path')
var input = path.resolve(__dirname, 'init-input.js')

var fs = require('fs')
var package = path.resolve(__dirname, 'package.json')
var pkg

fs.readFile(package, 'utf8', function (er, d) {
  if (er) ctx = {}
  try { ctx = JSON.parse(d); pkg = JSON.parse(d) }
  catch (e) { ctx = {} }

  ctx.dirname = path.dirname(package)
  ctx.basename = path.basename(ctx.dirname)
  if (!ctx.version) ctx.version = undefined

  // this should be replaced with the npm conf object
  ctx.config = {}

  console.error('ctx=', ctx)

  var pz = new PZ(input, ctx)

  pz.on('data', function (data) {
    console.error('pz data', data)
    if (!pkg) pkg = {}
    Object.keys(data).forEach(function (k) {
      if (data[k] !== undefined && data[k] !== null) pkg[k] = data[k]
    })
    console.error('package data %s', JSON.stringify(data, null, 2))
    fs.writeFile(package, JSON.stringify(pkg, null, 2), function (er) {
      if (er) throw er
      console.log('ok')
    })
  })
})
