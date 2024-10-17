import assert from 'node:assert/strict'
import { test } from 'node:test'

test('failing assertion', () => {
  assert.equal('apple', 'pear')
})
