
/**
 * Module dependencies.
 */

var assert = require('assert');

setTimeout(function(){
    exports['test async exports'] = function(){
        assert.ok('wahoo');
    };
}, 100);