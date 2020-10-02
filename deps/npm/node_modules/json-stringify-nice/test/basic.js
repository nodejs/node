const t = require('tap')
const stringify = require('../')

t.test('basic sorting operation with default 2-space indent', t => {
  t.plan(1)
  t.matchSnapshot(stringify({
    z: 1,
    y: 'z',
    obj: { a: {}, b: 'x' },
    a: { b: 1, a: 2},
    yy: 'a',
  }), 'mix of objects and out of order keys')
})

t.test('throws same error on cycles as JSON.stringify', t => {
  t.plan(1)
  const cycle = { a: { b: { c: {} } } }
  cycle.a.b.c = cycle.a
  try {
    JSON.stringify(cycle)
  } catch (builtinEr) {
    t.throws(() => stringify(cycle), builtinEr, 'same error as builtin')
  }
})

t.test('spaces can be set', t => {
  t.plan(5)
  const obj = {
    z: 1,
    y: 'z',
    obj: { a: {}, b: 'x' },
    a: { b: 1, a: 2},
    yy: 'a',
  }
  t.matchSnapshot(stringify(obj, 0, '\t'), 'tab')
  t.matchSnapshot(stringify(obj, null, ' ^_^ '), 'space face')
  t.matchSnapshot(stringify(obj, false, 3), 'the number 3')
  t.matchSnapshot(stringify(obj, false, ''), 'empty string')
  t.matchSnapshot(stringify(obj, false, false), 'boolean false')
})

t.test('replacer function is used', t => {
  t.plan(1)
  const obj = {
    z: 1,
    y: 'z',
    obj: { a: {}, b: 'x' },
    a: { b: 1, a: { nested: true} },
    yy: 'a',
  }
  const replacer = (key, val) =>
    key === 'a' ? { hello: '📞 yes', 'this is': '🐕', ...val }
    : val
  t.matchSnapshot(stringify(obj, replacer), 'replace a val with phone doggo')
})

t.test('sort keys explicitly with a preference list', t => {
  t.plan(1)
  const obj = {
    z: 1,
    y: 'z',
    obj: { a: {}, b: 'x' },
    a: { b: 1, a: { nested: true} },
    yy: 'a',
  }
  const preference = ['obj', 'z', 'yy']
  t.matchSnapshot(stringify(obj, preference), 'replace a val with preferences')
})
