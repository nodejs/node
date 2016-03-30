var pz = require('../promzard')

var path = require('path')
var file = path.resolve(__dirname, 'substack-input.js')
var buf = require('fs').readFileSync(file)
var ctx = { basename: path.basename(path.dirname(file)) }

pz.fromBuffer(buf, ctx, function (er, res) {
  if (er)
    throw er
  console.error(JSON.stringify(res, null, 2))
})
