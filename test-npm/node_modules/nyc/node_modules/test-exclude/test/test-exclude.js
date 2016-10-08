/* global describe, it */

const exclude = require('../')

require('chai').should()

describe('testExclude', function () {
  it('should always exclude node_modules folder', function () {
    exclude().shouldInstrument('./banana/node_modules/cat.js').should.equal(false)
  })

  it('ignores ./', function () {
    exclude().shouldInstrument('./test.js').should.equal(false)
  })

  it('does not instrument files outside cwd', function () {
    exclude().shouldInstrument('../foo.js').should.equal(false)
  })

  it('applies exclude rule ahead of include rule', function () {
    const e = exclude({
      include: ['test.js', 'foo.js'],
      exclude: ['test.js']
    })
    e.shouldInstrument('test.js').should.equal(false)
    e.shouldInstrument('foo.js').should.equal(true)
    e.shouldInstrument('banana.js').should.equal(false)
  })

  describe('pkgConf', function () {
    it('should load exclude rules from config key', function () {
      const e = exclude({
        configPath: './test/fixtures/exclude',
        configKey: 'a'
      })

      e.shouldInstrument('foo.js').should.equal(true)
      e.shouldInstrument('batman.js').should.equal(false)
      e.configFound.should.equal(true)
    })

    it('should load include rules from config key', function () {
      const e = exclude({
        configPath: './test/fixtures/include',
        configKey: 'b'
      })

      e.shouldInstrument('foo.js').should.equal(false)
      e.shouldInstrument('batman.js').should.equal(true)
      e.configFound.should.equal(true)
    })

    it('should not throw if a key is missing', function () {
      var e = exclude({
        configPath: './test/fixtures/include',
        configKey: 'c'
      })
      e.configFound.should.equal(false)
    })
  })
})
