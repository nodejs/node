module.exports = link

var gentleFS = require('gentle-fs')
var gentleFSOpts = require('../config/gentle-fs.js')

function link (from, to, gently, abs, cb) {
  return gentleFS.link(from, to, gentleFSOpts(gently, undefined, abs), cb)
}
