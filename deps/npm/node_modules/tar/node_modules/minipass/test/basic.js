const MiniPass = require('../')
const t = require('tap')
const EE = require('events').EventEmitter

t.test('some basic piping and writing', async t => {
  let mp = new MiniPass({ encoding: 'base64' })
  t.notOk(mp.flowing)
  mp.flowing = true
  t.notOk(mp.flowing)
  t.equal(mp.encoding, 'base64')
  mp.encoding = null
  t.equal(mp.encoding, null)
  t.equal(mp.readable, true)
  t.equal(mp.writable, true)
  t.equal(mp.write('hello'), false)
  let dest = new MiniPass()
  let sawDestData = false
  dest.once('data', chunk => {
    sawDestData = true
    t.isa(chunk, Buffer)
  })
  t.equal(mp.pipe(dest), dest, 'pipe returns dest')
  t.ok(sawDestData, 'got data becasue pipe() flushes')
  t.equal(mp.write('bye'), true, 'write() returns true when flowing')
  dest.pause()
  t.equal(mp.write('after pause'), false, 'false when dest is paused')
  t.equal(mp.write('after false'), false, 'false when not flowing')
  t.equal(dest.buffer.length, 1, '1 item is buffered in dest')
  t.equal(mp.buffer.length, 1, '1 item buffered in src')
  dest.resume()
  t.equal(dest.buffer.length, 0, 'nothing is buffered in dest')
  t.equal(mp.buffer.length, 0, 'nothing buffered in src')
})

t.test('unicode splitting', async t => {
  const butterfly = '🦋'
  const mp = new MiniPass({ encoding: 'utf8' })
  t.plan(2)
  t.equal(mp.encoding, 'utf8')
  mp.on('data', chunk => {
    t.equal(chunk, butterfly)
  })
  const butterbuf = new Buffer([0xf0, 0x9f, 0xa6, 0x8b])
  mp.write(butterbuf.slice(0, 1))
  mp.write(butterbuf.slice(1, 2))
  mp.write(butterbuf.slice(2, 3))
  mp.write(butterbuf.slice(3, 4))
  mp.end()
})

t.test('unicode splitting with setEncoding', async t => {
  const butterfly = '🦋'
  const mp = new MiniPass({ encoding: 'hex' })
  t.plan(4)
  t.equal(mp.encoding, 'hex')
  mp.setEncoding('hex')
  t.equal(mp.encoding, 'hex')
  mp.setEncoding('utf8')
  t.equal(mp.encoding, 'utf8')
  mp.on('data', chunk => {
    t.equal(chunk, butterfly)
  })
  const butterbuf = new Buffer([0xf0, 0x9f, 0xa6, 0x8b])
  mp.write(butterbuf.slice(0, 1))
  mp.write(butterbuf.slice(1, 2))
  mp.write(butterbuf.slice(2, 3))
  mp.write(butterbuf.slice(3, 4))
  mp.end()
})

t.test('base64 -> utf8 piping', t => {
  t.plan(1)
  const butterfly = '🦋'
  const mp = new MiniPass({ encoding: 'base64' })
  const dest = new MiniPass({ encoding: 'utf8' })
  mp.pipe(dest)
  let out = ''
  dest.on('data', c => out += c)
  dest.on('end', _ =>
    t.equal(new Buffer(out, 'base64').toString('utf8'), butterfly))
  mp.write(butterfly)
  mp.end()
})

t.test('utf8 -> base64 piping', t => {
  t.plan(1)
  const butterfly = '🦋'
  const mp = new MiniPass({ encoding: 'utf8' })
  const dest = new MiniPass({ encoding: 'base64' })
  mp.pipe(dest)
  let out = ''
  dest.on('data', c => out += c)
  dest.on('end', _ =>
    t.equal(new Buffer(out, 'base64').toString('utf8'), butterfly))
  mp.write(butterfly)
  mp.end()
})

t.test('read method', async t => {
  const butterfly = '🦋'
  const mp = new MiniPass({ encoding: 'utf8' })
  mp.on('data', c => t.equal(c, butterfly))
  mp.pause()
  mp.write(new Buffer(butterfly))
  t.equal(mp.read(5), null)
  t.equal(mp.read(0), null)
  t.same(mp.read(2), butterfly)
})

t.test('read with no args', async t => {
  t.test('buffer -> string', async t => {
    const butterfly = '🦋'
    const mp = new MiniPass({ encoding: 'utf8' })
    mp.on('data', c => t.equal(c, butterfly))
    mp.pause()
    const butterbuf = new Buffer(butterfly)
    mp.write(butterbuf.slice(0, 2))
    mp.write(butterbuf.slice(2))
    t.same(mp.read(), butterfly)
    t.equal(mp.read(), null)
  })

  t.test('buffer -> buffer', async t => {
    const butterfly = new Buffer('🦋')
    const mp = new MiniPass()
    mp.on('data', c => t.same(c, butterfly))
    mp.pause()
    mp.write(butterfly.slice(0, 2))
    mp.write(butterfly.slice(2))
    t.same(mp.read(), butterfly)
    t.equal(mp.read(), null)
  })

  t.test('string -> buffer', async t => {
    const butterfly = '🦋'
    const butterbuf = new Buffer(butterfly)
    const mp = new MiniPass()
    mp.on('data', c => t.same(c, butterbuf))
    mp.pause()
    mp.write(butterfly)
    t.same(mp.read(), butterbuf)
    t.equal(mp.read(), null)
  })

  t.test('string -> string', async t => {
    const butterfly = '🦋'
    const mp = new MiniPass({ encoding: 'utf8' })
    mp.on('data', c => t.equal(c, butterfly))
    mp.pause()
    mp.write(butterfly[0])
    mp.write(butterfly[1])
    t.same(mp.read(), butterfly)
    t.equal(mp.read(), null)
  })
})

t.test('partial read', async t => {
  const butterfly = '🦋'
  const mp = new MiniPass()
  const butterbuf = new Buffer(butterfly)
  mp.write(butterbuf.slice(0, 1))
  mp.write(butterbuf.slice(1, 2))
  mp.write(butterbuf.slice(2, 3))
  mp.write(butterbuf.slice(3, 4))
  t.equal(mp.read(5), null)
  t.equal(mp.read(0), null)
  t.same(mp.read(2), butterbuf.slice(0, 2))
  t.same(mp.read(2), butterbuf.slice(2, 4))
})

t.test('write after end', async t => {
  const mp = new MiniPass()
  let sawEnd = false
  mp.on('end', _ => sawEnd = true)
  mp.end()
  t.throws(_ => mp.write('nope'))
  t.notOk(sawEnd, 'should not get end event yet (not flowing)')
  mp.resume()
  t.ok(sawEnd, 'should get end event after resume()')
})

t.test('write cb', async t => {
  const mp = new MiniPass()
  let calledCb = false
  mp.write('ok', () => calledCb = true)
  t.ok(calledCb)
})

t.test('end with chunk', async t => {
  let out = ''
  const mp = new MiniPass({ encoding: 'utf8' })
  let sawEnd = false
  mp.on('end', _ => sawEnd = true)
  mp.addEventHandler('data', c => out += c)
  let endCb = false
  mp.end('ok', _ => endCb = true)
  t.equal(out, 'ok')
  t.ok(sawEnd, 'should see end event')
  t.ok(endCb, 'end cb should get called')
})

t.test('no drain if could not entirely drain on resume', async t => {
  const mp = new MiniPass()
  const dest = new MiniPass({ encoding: 'buffer' })
  t.equal(mp.write('foo'), false)
  t.equal(mp.write('bar'), false)
  t.equal(mp.write('baz'), false)
  t.equal(mp.write('qux'), false)
  mp.on('drain', _ => t.fail('should not drain'))
  mp.pipe(dest)
})

t.test('end with chunk pending', async t => {
  const mp = new MiniPass()
  t.equal(mp.write('foo'), false)
  t.equal(mp.write('626172', 'hex'), false)
  t.equal(mp.write('baz'), false)
  t.equal(mp.write('qux'), false)
  let sawEnd = false
  mp.on('end', _ => sawEnd = true)
  let endCb = false
  mp.end(_ => endCb = true)
  t.notOk(endCb, 'endcb should not happen yet')
  t.notOk(sawEnd, 'should not see end yet')
  let out = ''
  mp.on('data', c => out += c)
  t.ok(sawEnd, 'see end after flush')
  t.ok(endCb, 'end cb after flush')
  t.equal(out, 'foobarbazqux')
})

t.test('pipe to stderr does not throw', t => {
  const spawn = require('child_process').spawn
  const module = JSON.stringify(require.resolve('../'))
  const fs = require('fs')
  const file = __dirname + '/prog.js'
  fs.writeFileSync(file, `
    const MP = require(${module})
    const mp = new MP()
    mp.pipe(process.stderr)
    mp.end("hello")
  `)
  let err = ''
  return new Promise(res => {
    const child = spawn(process.execPath, [file])
    child.stderr.on('data', c => err += c)
    child.on('close', (code, signal) => {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(err, 'hello')
      fs.unlinkSync(file)
      res()
    })
  })
})

t.test('emit works with many args', t => {
  const mp = new MiniPass()
  t.plan(2)
  mp.on('foo', function (a, b, c, d, e, f, g) {
    t.same([a,b,c,d,e,f,g], [1,2,3,4,5,6,7])
    t.equal(arguments.length, 7)
  })
  mp.emit('foo', 1, 2, 3, 4, 5, 6, 7)
})

t.test('emit drain on resume, even if no flush', t => {
  const mp = new MiniPass()
  mp.encoding = 'utf8'

  const chunks = []
  class SlowStream extends EE {
    write (chunk) {
      chunks.push(chunk)
      setTimeout(_ => this.emit('drain'))
      return false
    }
    end () { return this.write() }
  }

  const ss = new SlowStream()

  mp.pipe(ss)
  t.ok(mp.flowing, 'flowing, because piped')
  t.equal(mp.write('foo'), false, 'write() returns false, backpressure')
  t.equal(mp.buffer.length, 0, 'buffer len is 0')
  t.equal(mp.flowing, false, 'flowing false, awaiting drain')
  t.same(chunks, ['foo'], 'chunk made it through')
  mp.once('drain', _ => {
    t.pass('received mp drain event')
    t.end()
  })
})

t.test('save close for end', t => {
  const mp = new MiniPass()
  let ended = false
  mp.on('close', _ => {
    t.equal(ended, true, 'end before close')
    t.end()
  })
  mp.on('end', _ => {
    t.equal(ended, false, 'only end once')
    ended = true
  })

  mp.emit('close')
  mp.end('foo')
  t.equal(ended, false, 'no end until flushed')
  mp.resume()
})

t.test('eos works', t => {
  const eos = require('end-of-stream')
  const mp = new MiniPass()

  eos(mp, er => {
    if (er)
      throw er
    t.end()
  })

  mp.emit('close')
  mp.end('foo')
  mp.resume()
})

t.test('bufferLength property', t => {
  const eos = require('end-of-stream')
  const mp = new MiniPass()
  mp.write('a')
  mp.write('a')
  mp.write('a')
  mp.write('a')
  mp.write('a')
  mp.write('a')

  t.equal(mp.bufferLength, 6)
  t.equal(mp.read(7), null)
  t.equal(mp.read(3).toString(), 'aaa')
  t.equal(mp.bufferLength, 3)
  t.equal(mp.read().toString(), 'aaa')
  t.equal(mp.bufferLength, 0)
  t.end()
})

t.test('emit resume event on resume', t => {
  const mp = new MiniPass()
  t.plan(3)
  mp.on('resume', _ => t.pass('got resume event'))
  mp.end('asdf')
  t.equal(mp.flowing, false, 'not flowing yet')
  mp.resume()
  t.equal(mp.flowing, true, 'flowing after resume')
})

t.test('objectMode', t => {
  const mp = new MiniPass({ objectMode: true })
  const a = { a: 1 }
  const b = { b: 1 }
  const out = []
  mp.on('data', c => out.push(c))
  mp.on('end', _ => {
    t.equal(out.length, 2)
    t.equal(out[0], a)
    t.equal(out[1], b)
    t.same(out, [ { a: 1 }, { b: 1 } ], 'objs not munged')
    t.end()
  })
  t.ok(mp.write(a))
  t.ok(mp.write(b))
  mp.end()
})

t.test('objectMode no encoding', t => {
  const mp = new MiniPass({
    objectMode: true,
    encoding: 'utf8'
  })
  t.equal(mp.encoding, null)
  const a = { a: 1 }
  const b = { b: 1 }
  const out = []
  mp.on('data', c => out.push(c))
  mp.on('end', _ => {
    t.equal(out.length, 2)
    t.equal(out[0], a)
    t.equal(out[1], b)
    t.same(out, [ { a: 1 }, { b: 1 } ], 'objs not munged')
    t.end()
  })
  t.ok(mp.write(a))
  t.ok(mp.write(b))
  mp.end()
})

t.test('objectMode read() and buffering', t => {
  const mp = new MiniPass({ objectMode: true })
  const a = { a: 1 }
  const b = { b: 1 }
  t.notOk(mp.write(a))
  t.notOk(mp.write(b))
  t.equal(mp.read(2), a)
  t.equal(mp.read(), b)
  t.end()
})

t.test('set encoding in object mode throws', async t =>
  t.throws(_ => new MiniPass({ objectMode: true }).encoding = 'utf8',
           new Error('cannot set encoding in objectMode')))

t.test('set encoding again throws', async t =>
  t.throws(_ => {
    const mp = new MiniPass({ encoding: 'hex' })
    mp.write('ok')
    mp.encoding = 'utf8'
  }, new Error('cannot change encoding')))

t.test('set encoding with existing buffer', async t => {
  const mp = new MiniPass()
  const butterfly = '🦋'
  const butterbuf = new Buffer(butterfly)
  mp.write(butterbuf.slice(0, 1))
  mp.write(butterbuf.slice(1, 2))
  mp.setEncoding('utf8')
  mp.write(butterbuf.slice(2))
  t.equal(mp.read(), butterfly)
})

t.test('end:false', async t => {
  t.plan(1)
  const mp = new MiniPass({ encoding: 'utf8' })
  const d = new MiniPass({ encoding: 'utf8' })
  d.end = () => t.threw(new Error('no end no exit no way out'))
  d.on('data', c => t.equal(c, 'this is fine'))
  mp.pipe(d, { end: false })
  mp.end('this is fine')
})
