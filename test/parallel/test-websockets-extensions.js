'use strict';
const Extensions = require('../../lib/Extensions');
const assert = require('assert');

describe('Extensions', function() {
  describe('parse', function() {
    it('should parse', function() {
      var extensions = Extensions.parse('foo');
      assert.deepEqual(extensions, { foo: [{}] });
    });

    it('should parse params', function() {
      var extensions = Extensions.parse('foo; bar; baz=1; bar=2');
      assert.deepEqual(extensions, {
        foo: [{ bar: [true, '2'], baz: ['1'] }]
      });
    });

    it('should parse multiple extensions', function() {
      var extensions = Extensions.parse('foo, bar; baz, foo; baz');
      assert.deepEqual(extensions, {
        foo: [{}, { baz: [true] }],
        bar: [{ baz: [true] }]
      });
    });

    it('should parse quoted params', function() {
      var extensions = Extensions.parse('foo; bar="hi"');
      assert.deepEqual(extensions, {
        foo: [{ bar: ['hi'] }]
      });
    });
  });

  describe('format', function() {
    it('should format', function() {
      var extensions = Extensions.format({ foo: {} });
      assert.deepEqual(extensions, 'foo');
    });

    it('should format params', function() {
      var extensions = Extensions.format({ foo: { bar: [true, 2], baz: 1 } });
      assert.deepEqual(extensions, 'foo; bar; bar=2; baz=1');
    });

    it('should format multiple extensions', function() {
      var extensions = Extensions.format({
        foo: [{}, { baz: true }],
        bar: { baz: true }
      });
      assert.deepEqual(extensions, 'foo, foo; baz, bar; baz');
    });
  });
});
