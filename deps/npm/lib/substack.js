module.exports = substack
var npm = require('./npm.js')

var isms = [
  '\u001b[32mbeep \u001b[35mboop\u001b[m',
  'Replace your configs with services',
  'SEPARATE ALL THE CONCERNS!',
  'MODULE ALL THE THINGS!',
  '\\o/',
  'but first, burritos',
  'full time mad scientist here',
  'c/,,\\'
]

function substack (args, cb) {
  var i = Math.floor(Math.random() * isms.length)
  console.log(isms[i])
  var c = args.shift()
  if (c) npm.commands[c](args, cb)
  else cb()
}
