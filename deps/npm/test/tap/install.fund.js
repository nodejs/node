'use strict'

const { test } = require('tap')
const { getPrintFundingReport } = require('../../lib/install/fund')

test('message when there are no funding found', (t) => {
  t.equal(
    getPrintFundingReport({}),
    '',
    'should not print any message if missing info'
  )
  t.equal(
    getPrintFundingReport({
      name: 'foo',
      version: '1.0.0',
      dependencies: {}
    }),
    '',
    'should not print any message if package has no dependencies'
  )
  t.equal(
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
  t.equal(
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
    }).replace(/[\r\n]+/g, '\n'),
    `\n1 package is looking for funding\n  run \`npm fund\` for details\n`,
    'should print single package message'
  )
  t.end()
})

test('print appropriate message for many packages', (t) => {
  t.equal(
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
    }).replace(/[\r\n]+/g, '\n'),
    `\n3 packages are looking for funding\n  run \`npm fund\` for details\n`,
    'should print many package message'
  )
  t.end()
})
