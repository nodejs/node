module.exports.drive = drive

var semver = require('semver')

function drive (input) {
  var stdin = process.stdin
  function emit (chunk, ms) {
    setTimeout(function () {
      stdin.emit('data', chunk)
    }, ms)
  }
  if (semver.gte(process.versions.node, '0.11.0')) {
    input.forEach(function (chunk) {
      stdin.push(chunk)
    })
  } else {
    stdin.once('readable', function () {
      var ms = 0
      input.forEach(function (chunk) {
        emit(chunk, ms += 50)
      })
    })
  }
}
