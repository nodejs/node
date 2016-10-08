var chai = require('chai')
chai.should()
var expect = chai.expect
var moquire = require('moquire')
var path = require('path')

var MODULE = '../'

describe('find-root', function () {

  it('recursively looks for package.json', function () {

    var checked = []
    var fs = {
      statSync: function (path) {
        checked.push(path)
        if (path === '/foo/package.json') {
          return {};
        } else {
          throw new Error();
        }
      }
    }

    var findRoot = moquire(MODULE, {fs: fs})

    findRoot('/foo/bar/baz')
      .should.equal('/foo')

    checked.should.deep.equal([
        '/foo/bar/baz/package.json',
        '/foo/bar/package.json',
        '/foo/package.json'
      ])

  })

  it('throws if not found', function () {

    var fs = {
      statSync: function (path) {
        throw new Error();
      }
    }

    var findRoot = moquire(MODULE, {fs: fs})

    expect(function () {
      findRoot('/foo/bar/baz/')
    }).to.throw('not found')

  })
})
