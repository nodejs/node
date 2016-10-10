'use strict';

var Ajv = require('ajv');
var defFunc = require('../keywords/propertyNames');
var defineKeywords = require('..');
var should = require('chai').should();


describe('keyword "propertyNames"', function() {
  var ajvs = [ new Ajv, new Ajv({allErrors: true}), new Ajv({v5: true}) ];
  defFunc(ajvs[0]);
  defineKeywords(ajvs[1], 'propertyNames');
  defineKeywords(ajvs[2]);

  ajvs.forEach(function (ajv, i) {
    it('should validate that all property names are valid #' + i, function() {
      var schema = {
        type: 'object',
        propertyNames: { format: 'email' }
      };
      var validData = {
        'foo@example.com': {},
        'bar.baz@email.example.com': {}
      };

      var invalidData = { 'foo': {} };

      ajv.validate(schema, {}) .should.equal(true);
      ajv.validate(schema, validData) .should.equal(true);
      ajv.validate(schema, invalidData) .should.equal(false);
    });
  });

  ajvs.forEach(function (ajv, i) {
    it('should throw when propertyNames schema is invalid #' + i, function() {
      [
        { propertyNames: { type: 1 } },
        { propertyNames: [] },
        { propertyNames: 1 },
        { propertyNames: 'foo' }
      ].forEach(function (schema) {
        should.throw(function() {
          ajv.compile(schema);
        });
      });
    });
  });

  it('should short circuit without allErrors option', function() {
    var ajv = ajvs[0];
    var schema = {
      type: 'object',
      propertyNames: { format: 'email' }
    };

    var invalidData = {
      'foo': {},
      'bar': {}
    };

    ajv.validate(schema, invalidData) .should.equal(false);
    ajv.errors .should.have.length(2);
  });

  it('should collect all errors with {allErrors: true} option', function() {
    var ajv = ajvs[1];
    var schema = {
      type: 'object',
      propertyNames: { format: 'email' }
    };

    var invalidData = {
      'foo': {},
      'bar': {}
    };

    ajv.validate(schema, invalidData) .should.equal(false);
    ajv.errors .should.have.length(4);
  });

  it('should allow v5 schemas with v5 option', function() {
    var ajv = ajvs[2];
    var schema = {
      type: 'object',
      propertyNames: { constant: 'foo' }
    };

    var validData = {
      'foo': {}
    };

    var invalidData = {
      'foo': {},
      'bar': {}
    };

    ajv.validate(schema, {}) .should.equal(true);
    ajv.validate(schema, validData) .should.equal(true);
    ajv.validate(schema, invalidData) .should.equal(false);
  });
});
