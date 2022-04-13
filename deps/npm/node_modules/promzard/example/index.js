var pz = require('../promzard')

var path = require('path')
var file = path.resolve(__dirname, 'substack-input.js')
var ctx = { basename: path.basename(path.dirname(file)) }

pz(file, ctx, function (er, res) {
  if (er)
    throw er
  console.error(JSON.stringify(res, null, 2))
})
