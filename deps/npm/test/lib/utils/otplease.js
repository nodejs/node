const { test } = require('tap')
const requireInject = require('require-inject')

const readUserInfo = {
  otp: async () => '1234',
}

const otplease = requireInject('../../../lib/utils/otplease.js', {
  '../../../lib/utils/read-user-info.js': readUserInfo,
})

test('prompts for otp for EOTP', async (t) => {
  const stdinTTY = process.stdin.isTTY
  const stdoutTTY = process.stdout.isTTY
  process.stdin.isTTY = true
  process.stdout.isTTY = true
  t.teardown(() => {
    process.stdin.isTTY = stdinTTY
    process.stdout.isTTY = stdoutTTY
  })

  let runs = 0
  const fn = async (opts) => {
    if (++runs === 1)
      throw Object.assign(new Error('nope'), { code: 'EOTP' })

    t.equal(opts.some, 'prop', 'carried original options')
    t.equal(opts.otp, '1234', 'received the otp')
    t.done()
  }

  await otplease({ some: 'prop' }, fn)
})

test('prompts for otp for 401', async (t) => {
  const stdinTTY = process.stdin.isTTY
  const stdoutTTY = process.stdout.isTTY
  process.stdin.isTTY = true
  process.stdout.isTTY = true
  t.teardown(() => {
    process.stdin.isTTY = stdinTTY
    process.stdout.isTTY = stdoutTTY
  })

  let runs = 0
  const fn = async (opts) => {
    if (++runs === 1) {
      throw Object.assign(new Error('nope'), {
        code: 'E401',
        body: 'one-time pass required',
      })
    }

    t.equal(opts.some, 'prop', 'carried original options')
    t.equal(opts.otp, '1234', 'received the otp')
    t.done()
  }

  await otplease({ some: 'prop' }, fn)
})

test('does not prompt for non-otp errors', async (t) => {
  const stdinTTY = process.stdin.isTTY
  const stdoutTTY = process.stdout.isTTY
  process.stdin.isTTY = true
  process.stdout.isTTY = true
  t.teardown(() => {
    process.stdin.isTTY = stdinTTY
    process.stdout.isTTY = stdoutTTY
  })

  const fn = async (opts) => {
    throw new Error('nope')
  }

  t.rejects(otplease({ some: 'prop' }, fn), { message: 'nope' }, 'rejects with the original error')
})

test('does not prompt if stdin or stdout is not a tty', async (t) => {
  const stdinTTY = process.stdin.isTTY
  const stdoutTTY = process.stdout.isTTY
  process.stdin.isTTY = false
  process.stdout.isTTY = false
  t.teardown(() => {
    process.stdin.isTTY = stdinTTY
    process.stdout.isTTY = stdoutTTY
  })

  const fn = async (opts) => {
    throw Object.assign(new Error('nope'), { code: 'EOTP' })
  }

  t.rejects(otplease({ some: 'prop' }, fn), { message: 'nope' }, 'rejects with the original error')
})
