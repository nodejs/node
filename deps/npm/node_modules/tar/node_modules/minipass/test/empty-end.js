const t = require('tap')
const MP = require('../')

t.test('emit end on resume', async t => {
  const list = []
  const mp = new MP()
  mp.on('end', _ => list.push('end'))
  mp.end()
  t.notOk(mp.emittedEnd)
  list.push('called end')
  mp.resume()
  t.ok(mp.emittedEnd)
  list.push('called resume')
  t.same(list, ['called end', 'end', 'called resume'])
})

t.test('emit end on read()', async t => {
  const list = []
  const mp = new MP()
  mp.on('end', _ => list.push('end'))
  mp.end()
  list.push('called end')

  mp.read()
  list.push('called read()')
  t.same(list, ['called end', 'end', 'called read()'])
})

t.test('emit end on data handler', async t => {
  const list = []
  const mp = new MP()
  mp.on('end', _ => list.push('end'))
  mp.end()
  list.push('called end')
  mp.on('data', _=>_)
  list.push('added data handler')
  t.same(list, ['called end', 'end', 'added data handler'])
})
