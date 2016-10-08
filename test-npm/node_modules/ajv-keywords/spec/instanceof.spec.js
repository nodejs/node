'use strict';

var Ajv = require('ajv');
var defFunc = require('../keywords/instanceof');
var defineKeywords = require('..');
var should = require('chai').should();


describe('keyword "instanceof"', function() {
  var ajvs = [ new Ajv, new Ajv, new Ajv ];
  defFunc(ajvs[0]);
  defineKeywords(ajvs[1], 'instanceof');
  defineKeywords(ajvs[2]);

  ajvs.forEach(function (ajv, i) {
    it('should validate classes #' + i, function() {
      ajv.validate({ instanceof: 'Object' }, {}) .should.equal(true);
      ajv.validate({ instanceof: 'Object' }, []) .should.equal(true);
      ajv.validate({ instanceof: 'Object' }, 'foo') .should.equal(false);
      ajv.validate({ instanceof: 'Array' }, {}) .should.equal(false);
      ajv.validate({ instanceof: 'Array' }, []) .should.equal(true);
      ajv.validate({ instanceof: 'Array' }, 'foo') .should.equal(false);
      ajv.validate({ instanceof: 'Function' }, function(){}) .should.equal(true);
      ajv.validate({ instanceof: 'Function' }, []) .should.equal(false);
      ajv.validate({ instanceof: 'Number' }, new Number(42)) .should.equal(true);
      ajv.validate({ instanceof: 'Number' }, 42) .should.equal(false);
      ajv.validate({ instanceof: 'Number' }, 'foo') .should.equal(false);
      ajv.validate({ instanceof: 'String' }, new String('foo')) .should.equal(true);
      ajv.validate({ instanceof: 'String' }, 'foo') .should.equal(false);
      ajv.validate({ instanceof: 'String' }, 42) .should.equal(false);
      ajv.validate({ instanceof: 'Date' }, new Date) .should.equal(true);
      ajv.validate({ instanceof: 'Date' }, {}) .should.equal(false);
      ajv.validate({ instanceof: 'RegExp' }, /.*/) .should.equal(true);
      ajv.validate({ instanceof: 'RegExp' }, {}) .should.equal(false);
      ajv.validate({ instanceof: 'Buffer' }, new Buffer('foo')) .should.equal(true);
      ajv.validate({ instanceof: 'Buffer' }, 'foo') .should.equal(false);
      ajv.validate({ instanceof: 'Buffer' }, {}) .should.equal(false);
    });

    it('should validate multiple classes #' + i, function() {
      ajv.validate({ instanceof: ['Array', 'Function'] }, []) .should.equal(true);
      ajv.validate({ instanceof: ['Array', 'Function'] }, function(){}) .should.equal(true);
      ajv.validate({ instanceof: ['Array', 'Function'] }, {}) .should.equal(false);
    });

    it('should allow adding classes #' + i, function() {
      function MyClass() {}

      should.throw(function() {
        ajv.compile({ instanceof: 'MyClass' });
      });

      defFunc.definition.CONSTRUCTORS.MyClass = MyClass;

      ajv.validate({ instanceof: 'MyClass' }, new MyClass) .should.equal(true);
      ajv.validate({ instanceof: 'Object' }, new MyClass) .should.equal(true);
      ajv.validate({ instanceof: 'MyClass' }, {}) .should.equal(false);

      delete defFunc.definition.CONSTRUCTORS.MyClass;
      ajv.removeSchema();

      should.throw(function() {
        ajv.compile({ instanceof: 'MyClass' });
      });

      defineKeywords.get('instanceof').definition.CONSTRUCTORS.MyClass = MyClass;

      ajv.validate({ instanceof: 'MyClass' }, new MyClass) .should.equal(true);
      ajv.validate({ instanceof: 'Object' }, new MyClass) .should.equal(true);
      ajv.validate({ instanceof: 'MyClass' }, {}) .should.equal(false);

      delete defFunc.definition.CONSTRUCTORS.MyClass;
      ajv.removeSchema();
    });

    it('should throw when not string or array is passed #' + i, function() {
      should.throw(function() {
        ajv.compile({ instanceof: 1 });
      });
    });
  });
});
