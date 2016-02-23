var editor = require('../');
editor(__dirname + '/beep.json', function (code, sig) {
    console.log('finished editing with code ' + code);
});
