'use strict'
const test = require('tap').test
const validateCIDRList = require('../../lib/token.js')._validateCIDRList

test('validateCIDRList', (t) => {
  t.plan(10)
  const single = ['127.0.0.0/24']
  const double = ['127.0.0.0/24', '192.168.0.0/16']
  const ipv6 = '2620:0:2d0:200::7/32'
  const ipv6Mixed = ['127.0.0/24', '2620:0:2d0:200::7/32', '192.168.0.0/16']
  t.doesNotThrow(() => t.isDeeply(validateCIDRList(single.join(',')), single), 'single string ipv4')
  t.doesNotThrow(() => t.isDeeply(validateCIDRList(single), single), 'single array ipv4')
  t.doesNotThrow(() => t.isDeeply(validateCIDRList(double.join(',')), double), 'double string ipv4')
  t.doesNotThrow(() => t.isDeeply(validateCIDRList(double), double), 'double array ipv4')
  t.throws(() => validateCIDRList(ipv6))
  t.throws(() => validateCIDRList(ipv6Mixed))
  t.done()
})
