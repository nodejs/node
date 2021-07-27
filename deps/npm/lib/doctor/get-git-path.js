var log = require('npmlog')
var which = require('which')

function getGitPath (cb) {
  var tracker = log.newItem('getGitPath', 1)
  tracker.info('getGitPath', 'Finding git in your PATH')
  which('git', function (err, path) {
    tracker.finish()
    cb(err, path)
  })
}

module.exports = getGitPath
