
/**
 * Module dependencies.
 */

var assert = require('assert');

module.exports = {
    'assert.eql()': function(){
        assert.equal(assert.deepEqual, assert.eql);
    },
    
    'assert.type()': function(){
        assert.type('foobar', 'string');
        assert.type(2, 'number');
        assert.throws(function(){
            assert.type([1,2,3], 'string');
        });
    },
    
    'assert.includes()': function(){
        assert.includes('some random string', 'dom');
        assert.throws(function(){
           assert.include('some random string', 'foobar');
        });

        assert.includes(['foo', 'bar'], 'bar');
        assert.includes(['foo', 'bar'], 'foo');
        assert.includes([1,2,3], 3);
        assert.includes([1,2,3], 2);
        assert.includes([1,2,3], 1);
        assert.throws(function(){
            assert.includes(['foo', 'bar'], 'baz');
        });
        
        assert.throws(function(){
            assert.includes({ wrong: 'type' }, 'foo');
        });
    },
    
    'assert.isNull()': function(){
        assert.isNull(null);
        assert.throws(function(){
            assert.isNull(undefined);
        });
        assert.throws(function(){
            assert.isNull(false);
        });
    },
    
    'assert.isUndefined()': function(){
        assert.isUndefined(undefined);
        assert.throws(function(){
            assert.isUndefined(null);
        });
        assert.throws(function(){
            assert.isUndefined(false);
        });
    },
    
    'assert.isNotNull()': function(){
        assert.isNotNull(false);
        assert.isNotNull(undefined);
        assert.throws(function(){
            assert.isNotNull(null);
        });
    },
    
    'assert.isDefined()': function(){
        assert.isDefined(false);
        assert.isDefined(null);
        assert.throws(function(){
            assert.isDefined(undefined);
        });
    },
    
    'assert.match()': function(){
        assert.match('foobar', /foo(bar)?/);
        assert.throws(function(){
            assert.match('something', /rawr/);
        });
    },
    
    'assert.length()': function(){
        assert.length('test', 4);
        assert.length([1,2,3,4], 4);
        assert.throws(function(){
            assert.length([1,2,3], 4);
        });
    }
};