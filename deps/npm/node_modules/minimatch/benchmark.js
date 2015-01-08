var m = require('./minimatch.js')
var pattern = "**/*.js"
var expand = require('brace-expansion')
var files = expand('x/y/z/{1..1000}.js')
var start = process.hrtime()

for (var i = 0; i < 1000; i++) {
  for (var f = 0; f < files.length; f++) {
    var res = m(pattern, files[f])
  }
  if (!(i%10)) process.stdout.write('.')
}
console.log('done')
var dur = process.hrtime(start)
console.log('%s ms', dur[0]*1e3 + dur[1]/1e6)
