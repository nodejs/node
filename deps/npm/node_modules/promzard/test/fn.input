var fs = require('fs')

module.exports = {
  "a": 1 + 2,
  "b": prompt('To be or not to be?', '!2b', function (s) {
    return s.toUpperCase() + '...'
  }),
  "c": {
    "x": prompt(function (x) { return x * 100 }),
    "y": tmpdir + "/y/file.txt"
  },
  a_function: function (cb) {
    fs.readFile(__filename, 'utf8', cb)
  },
  asyncPrompt: function (cb) {
    return cb(null, prompt('a prompt at any other time would smell as sweet'))
  }
}
