'use strict';

var Ajv = require('ajv');
var defineKeywords = require('..');
var should = require('chai').should();


describe('defineKeywords', function() {
  var ajv = new Ajv;

  it('should allow defining multiple keywords', function() {
    defineKeywords(ajv, ['typeof', 'instanceof']);
    ajv.validate({ typeof: 'undefined' }, undefined) .should.equal(true);
    ajv.validate({ typeof: 'undefined' }, {}) .should.equal(false);
    ajv.validate({ instanceof: 'Array' }, []) .should.equal(true);
    ajv.validate({ instanceof: 'Array' }, {}) .should.equal(false);
  });

  it('should throw when unknown keyword is passed', function() {
    should.throw(function() {
      defineKeywords(ajv, 'unknownKeyword');
    });

    should.throw(function() {
      defineKeywords(ajv, ['typeof', 'unknownKeyword']);
    });
  });
});
