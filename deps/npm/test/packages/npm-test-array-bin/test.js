var c = require('child_process').spawn('array-bin', [], {
    env: process.env }).on('close', function (code) {
  if (code) throw new Error('exited badly with code = ' + code)
})
c.stdout.pipe(process.stdout)
c.stderr.pipe(process.stderr)
