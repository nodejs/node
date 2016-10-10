var exit = process.argv[2] || 0

var onSignalExit = require('../../')

onSignalExit(function (code, signal) {
  // some signals don't always get recognized properly, because
  // they have the same numeric code.
  if (wanted[1] === true) {
    signal = !!signal
  }
  console.log('%j', {
    found: [ code, signal ],
    wanted: wanted
  })
})

var wanted
if (isNaN(exit)) {
  switch (exit) {
    case 'SIGIOT':
    case 'SIGUNUSED':
    case 'SIGPOLL':
      wanted = [ null, true ]
      break
    default:
      wanted = [ null, exit ]
      break
  }

  try {
    process.kill(process.pid, exit)
    setTimeout(function () {}, 1000)
  } catch (er) {
    wanted = [ 0, null ]
  }

} else {
  exit = +exit
  wanted = [ exit, null ]
  // If it's explicitly requested 0, then explicitly call it.
  // "no arg" = "exit naturally"
  if (exit || process.argv[2]) {
    process.exit(exit)
  }
}
