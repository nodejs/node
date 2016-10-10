'use strict';

var Ajv = require('ajv');
var ajvPack = require('ajv-pack');
var defFunc = require('../keywords/range');
var defineKeywords = require('..');
var should = require('chai').should();


describe('keyword "range"', function() {
  var ajvs = [ new Ajv, new Ajv, new Ajv, ajvPack.instance(new Ajv({sourceCode: true})) ];
  defFunc(ajvs[0]);
  defineKeywords(ajvs[1], 'range');
  defineKeywords(ajvs[2]);
  defFunc(ajvs[3]);

  ajvs.forEach(function (ajv, i) {
    it('should validate that value is in range #' + i, function() {
      var schema = { range: [1, 3] };
      ajv.validate(schema, 1) .should.equal(true);
      ajv.validate(schema, 2) .should.equal(true);
      ajv.validate(schema, 3) .should.equal(true);
      ajv.validate(schema, 0.99) .should.equal(false);
      ajv.validate(schema, 3.01) .should.equal(false);

      ajv.validate({ range: [1, 1] }, 1) .should.equal(true);

      var schemaExcl = { range: [1, 3], exclusiveRange: true };
      ajv.validate(schemaExcl, 1) .should.equal(false);
      ajv.validate(schemaExcl, 2) .should.equal(true);
      ajv.validate(schemaExcl, 3) .should.equal(false);
      ajv.validate(schemaExcl, 1.01) .should.equal(true);
      ajv.validate(schemaExcl, 2.99) .should.equal(true);
    });
  });

  ajvs.forEach(function (ajv, i) {
    it('should throw when range schema is invalid #' + i, function() {
      [
        { range: [1, '3'] },
        { range: [1] },
        { range: [1, 2, 3] },
        { range: {} },
        { range: [3, 1] },

        { range: [1, 3], exclusiveRange: 'true' },
        { range: [1, 1], exclusiveRange: true }
      ].forEach(function (schema) {
        should.throw(function() {
          ajv.compile(schema);
        });
      });
    });
  });
});
