// This fixture is not used in any tests.  It is here merely as a way to
// do research into the various signal behaviors on Linux and Darwin.
// Run with no args to cycle through every signal type.  Run with a signal
// arg to learn about how that signal behaves.

if (process.argv[2]) {
  child(process.argv[2])
} else {
  var signals = [
    'SIGABRT',
    'SIGALRM',
    'SIGBUS',
    'SIGCHLD',
    'SIGCLD',
    'SIGCONT',
    'SIGEMT',
    'SIGFPE',
    'SIGHUP',
    'SIGILL',
    'SIGINFO',
    'SIGINT',
    'SIGIO',
    'SIGIOT',
    'SIGKILL',
    'SIGLOST',
    'SIGPIPE',
    'SIGPOLL',
    'SIGPROF',
    'SIGPWR',
    'SIGQUIT',
    'SIGSEGV',
    'SIGSTKFLT',
    'SIGSTOP',
    'SIGSYS',
    'SIGTERM',
    'SIGTRAP',
    'SIGTSTP',
    'SIGTTIN',
    'SIGTTOU',
    'SIGUNUSED',
    'SIGURG',
    'SIGUSR1',
    'SIGUSR2',
    'SIGVTALRM',
    'SIGWINCH',
    'SIGXCPU',
    'SIGXFSZ'
  ]

  var spawn = require('child_process').spawn
  ;(function test (signal) {
    if (!signal) {
      return
    }
    var child = spawn(process.execPath, [__filename, signal], { stdio: 'inherit' })
    var timer = setTimeout(function () {
      console.log('requires SIGCONT')
      process.kill(child.pid, 'SIGCONT')
    }, 750)

    child.on('close', function (code, signal) {
      console.log('code=%j signal=%j\n', code, signal)
      clearTimeout(timer)
      test(signals.pop())
    })
  })(signals.pop())
}

function child (signal) {
  console.log('signal=%s', signal)

  // set a timeout so we know whether or not the process terminated.
  setTimeout(function () {
    console.log('not terminated')
  }, 200)

  process.on('exit', function (code) {
    console.log('emit exit code=%j', code)
  })

  try {
    process.on(signal, function fn () {
      console.log('signal is catchable', signal)
      process.removeListener(signal, fn)
      setTimeout(function () {
        console.error('signal again')
        process.kill(process.pid, signal)
      })
    })
  } catch (er) {
    console.log('not listenable')
  }

  try {
    process.kill(process.pid, signal)
  } catch (er) {
    console.log('not issuable')
  }
}
