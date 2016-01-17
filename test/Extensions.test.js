var Extensions = require('../lib/Extensions');
require('should');

describe('Extensions', function() {
  describe('parse', function() {
    it('should parse', function() {
      var extensions = Extensions.parse('foo');
      extensions.should.eql({ foo: [{}] });
    });

    it('should parse params', function() {
      var extensions = Extensions.parse('foo; bar; baz=1; bar=2');
      extensions.should.eql({
        foo: [{ bar: [true, '2'], baz: ['1'] }]
      });
    });

    it('should parse multiple extensions', function() {
      var extensions = Extensions.parse('foo, bar; baz, foo; baz');
      extensions.should.eql({
        foo: [{}, { baz: [true] }],
        bar: [{ baz: [true] }]
      });
    });

    it('should parse quoted params', function() {
      var extensions = Extensions.parse('foo; bar="hi"');
      extensions.should.eql({
        foo: [{ bar: ['hi'] }]
      });
    });
  });

  describe('format', function() {
    it('should format', function() {
      var extensions = Extensions.format({ foo: {} });
      extensions.should.eql('foo');
    });

    it('should format params', function() {
      var extensions = Extensions.format({ foo: { bar: [true, 2], baz: 1 } });
      extensions.should.eql('foo; bar; bar=2; baz=1');
    });

    it('should format multiple extensions', function() {
      var extensions = Extensions.format({
        foo: [{}, { baz: true }],
        bar: { baz: true }
      });
      extensions.should.eql('foo, foo; baz, bar; baz');
    });
  });
});
