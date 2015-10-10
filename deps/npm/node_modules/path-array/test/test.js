
/**
 * Module dependencies.
 */

var assert = require('assert');
var PathArray = require('../');
var delimiter = require('path').delimiter || ':';

describe('PathArray', function () {
  it('should use `process.env` by default', function () {
    var p = new PathArray();
    assert.equal(p._env, process.env);
  });
  it('should return the $PATH string for .toString()', function () {
    var p = new PathArray();
    assert.equal(p.toString(), process.env.PATH);
  });
  it('should accept an arbitrary `env` object', function () {
    var env = { PATH: '/foo' + delimiter + '/bar' };
    var p = new PathArray(env);
    assert.equal(p.toString(), env.PATH);
  });
  it('should work for [n] getter syntax', function () {
    var env = { PATH: '/foo' + delimiter + '/bar' };
    var p = new PathArray(env);
    assert.equal('/foo', p[0]);
    assert.equal('/bar', p[1]);
  });
  it('should work for [n]= setter syntax', function () {
    var env = { PATH: '/foo' + delimiter + '/bar' };
    var p = new PathArray(env);
    p[0] = '/baz';
    assert.equal('/baz' + delimiter + '/bar', env.PATH);
  });
  it('should work with .push()', function () {
    var env = { PATH: '/foo' + delimiter + '/bar' };
    var p = new PathArray(env);
    p.push('/baz');
    assert.equal('/foo' + delimiter + '/bar' + delimiter + '/baz', env.PATH);
  });
  it('should work with .shift()', function () {
    var env = { PATH: '/foo' + delimiter + '/bar' };
    var p = new PathArray(env);
    assert.equal('/foo', p.shift());
    assert.equal('/bar', env.PATH);
  });
  it('should work with .pop()', function () {
    var env = { PATH: '/foo' + delimiter + '/bar' };
    var p = new PathArray(env);
    assert.equal('/bar', p.pop());
    assert.equal('/foo', env.PATH);
  });
  it('should work with .unshift()', function () {
    var env = { PATH: '/foo' + delimiter + '/bar' };
    var p = new PathArray(env);
    p.unshift('/baz');
    assert.equal('/baz' + delimiter + '/foo' + delimiter + '/bar', env.PATH);
  });
  it('should be able to specify property name to use with second argument', function () {
    var env = { PYTHONPATH: '/foo' };
    var p = new PathArray(env, 'PYTHONPATH');
    assert.equal(1, p.length);
    p.push('/baz');
    assert.equal(2, p.length);
    assert.equal('/foo' + delimiter + '/baz', env.PYTHONPATH);
  });
});
