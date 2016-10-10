'use strict';

var Ajv = require('ajv');
var ajvPack = require('ajv-pack');
var defFunc = require('../keywords/typeof');
var defineKeywords = require('..');
var should = require('chai').should();


describe('keyword "typeof"', function() {
  var ajvs = [ new Ajv, new Ajv, new Ajv, ajvPack.instance(new Ajv({sourceCode: true})) ];
  defFunc(ajvs[0]);
  defineKeywords(ajvs[1], 'typeof');
  defineKeywords(ajvs[2]);
  defFunc(ajvs[3]);

  ajvs.forEach(function (ajv, i) {
    it('should validate value types #' + i, function() {
      ajv.validate({ typeof: 'undefined' }, undefined) .should.equal(true);
      ajv.validate({ typeof: 'undefined' }, null) .should.equal(false);
      ajv.validate({ typeof: 'undefined' }, 'foo') .should.equal(false);
      ajv.validate({ typeof: 'function' }, function(){}) .should.equal(true);
      ajv.validate({ typeof: 'function' }, {}) .should.equal(false);
      ajv.validate({ typeof: 'object' }, {}) .should.equal(true);
      ajv.validate({ typeof: 'object' }, null) .should.equal(true);
      ajv.validate({ typeof: 'object' }, 'foo') .should.equal(false);
      ajv.validate({ typeof: 'symbol' }, Symbol()) .should.equal(true);
      ajv.validate({ typeof: 'symbol' }, {}) .should.equal(false);
    });

    it('should validate multiple types #' + i, function() {
      ajv.validate({ typeof: ['string', 'function'] }, 'foo') .should.equal(true);
      ajv.validate({ typeof: ['string', 'function'] }, function(){}) .should.equal(true);
      ajv.validate({ typeof: ['string', 'function'] }, {}) .should.equal(false);
    });

    it('should throw when unknown type is passed #' + i, function() {
      should.throw(function() {
        ajv.compile({ typeof: 'unknownType' });
      });

      should.throw(function() {
        ajv.compile({ typeof: ['string', 'unknownType'] });
      });
    });

    it('should throw when not string or array is passed #' + i, function() {
      should.throw(function() {
        ajv.compile({ typeof: 1 });
      });
    });
  });
});
