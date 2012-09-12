var test = require('testling');
var vm = require('./');

test('vmRunInNewContext', function (t) {
    t.plan(5);
    
    t.equal(vm.runInNewContext('a + 5', { a : 100 }), 105);
    
    (function () {
        var vars = { x : 10 };
        t.equal(vm.runInNewContext('x++', vars), 10);
        t.equal(vars.x, 11);
    })();
    
    (function () {
        var vars = { x : 10 };
        t.equal(vm.runInNewContext('var y = 3; y + x++', vars), 13);
        t.equal(vars.x, 11);
    })();
    
    t.end();
});
