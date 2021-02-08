const timers = Object.create(null)

process.on('time', name => {
  if (timers[name])
    throw new Error('conflicting timer! ' + name)
  timers[name] = process.hrtime()
})

process.on('timeEnd', name => {
  if (!timers[name])
    throw new Error('timer not started! ' + name)
  const res = process.hrtime(timers[name])
  delete timers[name]
  console.error(`${process.pid} ${name}`, res[0] * 1e3 + res[1] / 1e6)
})

process.on('exit', () => {
  for (const name of Object.keys(timers)) {
    console.error('Dangling timer: ', name)
    process.exitCode = 1
  }
})
