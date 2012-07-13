var spawn = require('child_process').spawn
  , exitCode = 0
  ;

var tests = [
    'test-body.js'
  , 'test-cookie.js'
  , 'test-cookiejar.js'
  , 'test-defaults.js'
  , 'test-errors.js'
  , 'test-headers.js'
  , 'test-httpModule.js'
  , 'test-https.js'
  , 'test-https-strict.js'
  , 'test-oauth.js'
  , 'test-pipes.js'
  , 'test-pool.js'
  , 'test-proxy.js'
  , 'test-qs.js'
  , 'test-redirect.js'
  , 'test-timeout.js'
  , 'test-toJSON.js'
  , 'test-tunnel.js'
] 

var next = function () {
  if (tests.length === 0) process.exit(exitCode);
  
  var file = tests.shift()
  console.log(file)
  var proc = spawn('node', [ 'tests/' + file ])
  proc.stdout.pipe(process.stdout)
  proc.stderr.pipe(process.stderr)
  proc.on('exit', function (code) {
  	exitCode += code || 0
  	next()
  })
}
next()
