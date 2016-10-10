var signal = process.argv[2]
var gens = +process.argv[3] || 0

if (!signal || !isNaN(signal)) {
  throw new Error('signal not provided')
}

var spawn = require('child_process').spawn
var file = require.resolve('./awaiter.js')
console.error(process.pid, signal, gens)

if (gens > 0) {
  file = __filename
}

var child = spawn(process.execPath, [file, signal, gens - 1], {
  stdio: [ 0, 'pipe', 'pipe' ]
})

if (!gens) {
  child.stderr.on('data', function () {
    child.kill(signal)
  })
}

var result = ''
child.stdout.on('data', function (c) {
  result += c
})

child.on('close', function (code, sig) {
  try {
    result = JSON.parse(result)
  } catch (er) {
    console.log('%j', {
      error: 'failed to parse json\n' + er.message,
      result: result,
      pid: process.pid,
      child: child.pid,
      gens: gens,
      expect: [ null, signal ],
      actual: [ code, sig ]
    })
    return
  }
  if (result.wanted[1] === true) {
    sig = !!sig
  }
  result.external = result.external || [ code, sig ]
  console.log('%j', result)
})
