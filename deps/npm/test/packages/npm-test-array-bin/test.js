require('child_process').exec('array-bin', { env: process.env },
    function (err) {
  if (err && err.code) throw new Error('exited badly with code = ' + err.code)
})
