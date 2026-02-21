const t = require('tap')

const main = () => {
  if (process.argv[2] === 'polyfill-all-settled') {
    Promise.allSettled = null
    runTests()
  } else if (process.argv[2] === 'native-all-settled') {
    Promise.allSettled = Promise.allSettled || (
      promises => {
        const reflections = []
        for (let i = 0; i < promises.length; i++) {
          reflections[i] = Promise.resolve(promises[i]).then(value => ({
            status: 'fulfilled',
            value,
          }), reason => ({
            status: 'rejected',
            reason,
          }))
        }
        return Promise.all(reflections)
      }
    )
    runTests()
  } else {
    t.spawn(process.execPath, [__filename, 'polyfill-all-settled'])
    t.spawn(process.execPath, [__filename, 'native-all-settled'])
  }
}

const runTests = () => {
  const lateFail = require('../')

  t.test('fail only after all promises resolve', t => {
    let resolvedSlow = false
    const fast = () => Promise.reject('nope')
    const slow = () => new Promise(res => setTimeout(res, 100))
      .then(() => resolvedSlow = true)

    // throw some holes and junk in the array to verify that we handle it
    return t.rejects(lateFail([fast(),,,,slow(), null, {not: 'a promise'},,,]))
      .then(() => t.equal(resolvedSlow, true, 'resolved slow before failure'))
  })

  t.test('works just like Promise.all() otherwise', t => {
    const one = () => Promise.resolve(1)
    const two = () => Promise.resolve(2)
    const tre = () => Promise.resolve(3)
    const fur = () => Promise.resolve(4)
    const fiv = () => Promise.resolve(5)
    const six = () => Promise.resolve(6)
    const svn = () => Promise.resolve(7)
    const eit = () => Promise.resolve(8)
    const nin = () => Promise.resolve(9)
    const ten = () => Promise.resolve(10)
    const expect = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    const all = Promise.all([
      one(),
      two(),
      tre(),
      fur(),
      fiv(),
      six(),
      svn(),
      eit(),
      nin(),
      ten(),
    ])
    const late = lateFail([
      one(),
      two(),
      tre(),
      fur(),
      fiv(),
      six(),
      svn(),
      eit(),
      nin(),
      ten(),
    ])

    return Promise.all([all, late]).then(([all, late]) => {
      t.strictSame(all, expect)
      t.strictSame(late, expect)
    })
  })
}

main()
