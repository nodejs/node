'use strict';

var Ajv = require('ajv');
var ajvPack = require('ajv-pack');
var defFunc = require('../keywords/regexp');
var defineKeywords = require('..');
var should = require('chai').should();


describe('keyword "regexp"', function() {
  var ajvs = [ new Ajv, new Ajv({allErrors: true}), new Ajv, ajvPack.instance(new Ajv({sourceCode: true})) ];
  defFunc(ajvs[0]);
  defineKeywords(ajvs[1], 'regexp');
  defineKeywords(ajvs[2]);
  defFunc(ajvs[3]);

  ajvs.forEach(function (ajv, i) {
    it('should validate that values match regular expressions with flags #' + i, function() {
      var schema = {
        type: 'object',
        properties: {
          foo: { regexp: '/foo/i' },
          bar: { regexp: { pattern: 'bar', flags: 'i' } }
        }
      };

      var validData = {
        foo: 'Food',
        bar: 'Barmen'
      };

      var invalidData = {
        foo: 'fog',
        bar: 'bad'
      };

      ajv.validate(schema, {}) .should.equal(true);
      ajv.validate(schema, validData) .should.equal(true);
      ajv.validate(schema, invalidData) .should.equal(false);
    });
  });

  ajvs.forEach(function (ajv, i) {
    it('should throw when regexp schema is invalid #' + i, function() {
      [
        { regexp: '/foo' }, // invalid regexp
        { regexp: '/foo/a' }, // invalid regexp 2
        { regexp: { pattern: '[a-z' } }, // invalid regexp
        { regexp: { pattern: '[a-z]', flags: 'a' } }, // invalid flag
        { regexp: { flag: 'i' } }, // missing pattern
        { regexp: { pattern: '[a-z]', flag: 'i', foo: 1 } }, // extra property
        { regexp: 1 }, // incorrect type
        { regexp: { pattern: 1, flags: 'i' } } // incorrect type
      ].forEach(function (schema) {
        should.throw(function() {
          ajv.compile(schema);
        });
      });
    });
  });
});
