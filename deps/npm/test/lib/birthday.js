const { test } = require('tap')
const requireInject = require('require-inject')

test('birthday (nope)', (t) => {
  t.plan(1)
  const B = global[Buffer.from([66, 117, 102, 102, 101, 114])]
  const f = B.from([102, 114, 111, 109])
  const D = global[B[f]([68, 97, 116, 101])]
  const _6 = B[f]([98, 97, 115, 101, 54, 52]) + ''
  const l = B[f]('dG9TdHJpbmc=', _6)
  class FD extends D {
    [B[f]('Z2V0VVRDTW9udGg=', _6)[l]()] () {
      return 7
    }
  }
  global[B[f]([68, 97, 116, 101])] = FD
  const consoleLog = console.log
  console.log = () => undefined
  t.tearDown(() => {
    global[B[f]([68, 97, 116, 101])] = D
    console.log = consoleLog
  })
  const birthday = requireInject('../../lib/birthday', {})
  birthday([], (err) => {
    t.match(err, 'try again', 'not telling you the secret that easily are we?')
  })
})

test('birthday (nope again)', (t) => {
  t.plan(1)
  const B = global[Buffer.from([66, 117, 102, 102, 101, 114])]
  const f = B.from([102, 114, 111, 109])
  const D = global[B[f]([68, 97, 116, 101])]
  const _6 = B[f]([98, 97, 115, 101, 54, 52]) + ''
  const l = B[f]('dG9TdHJpbmc=', _6)
  class FD extends D {
    [B[f]('Z2V0RnVsbFllYXI=', _6)[l]()] () {
      const d = new D()
      return d[B[f]('Z2V0RnVsbFllYXI=', _6)[l]()]() + 1
    }

    [B[f]('Z2V0VVRDTW9udGg=', _6)[l]()] () {
      return 9
    }
  }
  global[B[f]([68, 97, 116, 101])] = FD
  const consoleLog = console.log
  console.log = () => undefined
  t.tearDown(() => {
    global[B[f]([68, 97, 116, 101])] = D
    console.log = consoleLog
  })
  const birthday = requireInject('../../lib/birthday', {})
  birthday([], (err) => {
    t.match(err, 'try again', 'not telling you the secret that easily are we?')
  })
})

test('birthday (yup)', (t) => {
  t.plan(1)
  const B = global[Buffer.from([66, 117, 102, 102, 101, 114])]
  const f = B.from([102, 114, 111, 109])
  const D = global[B[f]([68, 97, 116, 101])]
  const _6 = B[f]([98, 97, 115, 101, 54, 52]) + ''
  const l = B[f]('dG9TdHJpbmc=', _6)
  class FD extends D {
    [B[f]('Z2V0VVRDTW9udGg=', _6)[l]()] () {
      return 8
    }

    [B[f]('Z2V0VVRDRGF0ZQ==', _6)[l]()] () {
      return 29
    }
  }
  global[B[f]([68, 97, 116, 101])] = FD
  const consoleLog = console.log
  console.log = () => undefined
  t.tearDown(() => {
    global[B[f]([68, 97, 116, 101])] = D
    console.log = consoleLog
  })
  const birthday = requireInject('../../lib/birthday', {})
  birthday([], (err) => {
    t.ifError(err, 'npm birthday')
  })
})
