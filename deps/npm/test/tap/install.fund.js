'use strict'

const { EOL } = require('os')
const { test } = require('tap')
const { getPrintFundingReport } = require('../../lib/install/fund')

test('message when there are no funding found', (t) => {
  t.deepEqual(
    getPrintFundingReport({}),
    '',
    'should not print any message if missing info'
  )
  t.deepEqual(
    getPrintFundingReport({
      name: 'foo',
      version: '1.0.0',
      dependencies: {}
    }),
    '',
    'should not print any message if package has no dependencies'
  )
  t.deepEqual(
    getPrintFundingReport({
      fund: true,
      idealTree: {
        name: 'foo',
        version: '1.0.0',
        dependencies: {
          bar: {},
          lorem: {}
        }
      }
    }),
    '',
    'should not print any message if no package has funding info'
  )
  t.end()
})

test('print appropriate message for a single package', (t) => {
  t.deepEqual(
    getPrintFundingReport({
      fund: true,
      idealTree: {
        name: 'foo',
        version: '1.0.0',
        children: [
          {
            package: {
              name: 'bar',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' }
            }
          }
        ]
      }
    }),
    `${EOL}1 package is looking for funding.${EOL}Run "npm fund" to find out more.`,
    'should print single package message'
  )
  t.end()
})

test('print appropriate message for many packages', (t) => {
  t.deepEqual(
    getPrintFundingReport({
      fund: true,
      idealTree: {
        name: 'foo',
        version: '1.0.0',
        children: [
          {
            package: {
              name: 'bar',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' }
            }
          },
          {
            package: {
              name: 'lorem',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' }
            }
          },
          {
            package: {
              name: 'ipsum',
              version: '1.0.0',
              funding: { type: 'foo', url: 'http://example.com' }
            }
          }
        ]
      }
    }),
    `${EOL}3 packages are looking for funding.${EOL}Run "npm fund" to find out more.`,
    'should print many package message'
  )
  t.end()
})
