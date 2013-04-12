var spawn = require('child_process').spawn
  , exitCode = 0
  , timeout = 10000
  , fs = require('fs')
  ;

fs.readdir(__dirname, function (e, files) {
  if (e) throw e

  var tests = files.filter(function (f) {return f.slice(0, 'test-'.length) === 'test-'})

  var next = function () {
    if (tests.length === 0) process.exit(exitCode);

    var file = tests.shift()
    console.log(file)
    var proc = spawn('node', [ 'tests/' + file ])

    var killed = false
    var t = setTimeout(function () {
      proc.kill()
      exitCode += 1
      console.error(file + ' timeout')
      killed = true
    }, timeout)

    proc.stdout.pipe(process.stdout)
    proc.stderr.pipe(process.stderr)
    proc.on('exit', function (code) {
      if (code && !killed) console.error(file + ' failed')
      exitCode += code || 0
      clearTimeout(t)
      next()
    })
  }
  next()
    
})


