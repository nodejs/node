const t = require('tap')
const mockGlobals = require('../../fixtures/mock-globals')

const readUserInfo = {
  otp: async () => '1234',
}

const otplease = t.mock('../../../lib/utils/otplease.js', {
  '../../../lib/utils/read-user-info.js': readUserInfo,
})

t.test('returns function results on success', async (t) => {
  const fn = () => 'test string'
  const result = await otplease({}, fn)
  t.equal('test string', result)
})

t.test('returns function results on otp success', async (t) => {
  mockGlobals(t, {
    'process.stdin': { isTTY: true },
    'process.stdout': { isTTY: true },
  })
  const fn = ({ otp }) => {
    if (otp) {
      return 'success'
    }
    throw Object.assign(new Error('nope'), { code: 'EOTP' })
  }
  const result = await otplease({}, fn)
  t.equal('success', result)
})

t.test('prompts for otp for EOTP', async (t) => {
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
      throw Object.assign(new Error('nope'), { code: 'EOTP' })
    }

    t.equal(opts.some, 'prop', 'carried original options')
    t.equal(opts.otp, '1234', 'received the otp')
    t.end()
  }

  await otplease({ some: 'prop' }, fn)
})

t.test('prompts for otp for 401', async (t) => {
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
    t.end()
  }

  await otplease({ some: 'prop' }, fn)
})

t.test('does not prompt for non-otp errors', async (t) => {
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

t.test('does not prompt if stdin or stdout is not a tty', async (t) => {
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
