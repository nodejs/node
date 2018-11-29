var readdirSync = require('fs').readdirSync
var spawnSync = require('child_process').spawnSync
var extname = require('path').extname

var files = readdirSync(__dirname)
files.forEach(function(file){
  if (extname(file) !== '.js' || file === 'run.js')
    return
  console.log(`*** ${file} ***`)
  var result = spawnSync(process.argv0, [file], { stdio: 'inherit', cwd: __dirname} )
  if (result.status !== 0)
    process.exit(result.status)
})
