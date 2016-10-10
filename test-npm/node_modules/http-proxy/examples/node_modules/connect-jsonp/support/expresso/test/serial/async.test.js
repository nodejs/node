
var assert = require('assert')
  , setup = 0
  , order = [];

module.exports = {
    setup: function(done){
        ++setup;
        done();
    },

    a: function(done){
        assert.equal(1, setup);
        order.push('a');
        setTimeout(function(){
            done();
        }, 500);
    },
    
    b: function(done){
        assert.equal(2, setup);
        order.push('b');
        setTimeout(function(){
            done();
        }, 200);
    },
    
    c: function(done){
        assert.equal(3, setup);
        order.push('c');
        setTimeout(function(){
            done();
        }, 1000);
    },

    d: function(){
        assert.eql(order, ['a', 'b', 'c']);
    }
};