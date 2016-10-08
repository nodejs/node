
/**
 * Module dependencies.
 */

var assert = require('assert')
  , bar = require('bar');

module.exports = {
    'bar()': function(){
        assert.equal('bar', bar.bar());
    }
};