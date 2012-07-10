require('child_process').spawn('dir-bin', [], {
    env: process.env }).on('exit', function (code) {
  if (code) throw new Error('exited badly with code = ' + code)
})

