require('child_process').exec('dir-bin', { stdio: 'pipe',
    env: process.env }, function (err) {
  if (err && err.code) throw new Error('exited badly with code = ' + err.code)
})
