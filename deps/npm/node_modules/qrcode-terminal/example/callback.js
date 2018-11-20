var qrcode = require('../lib/main');
qrcode.generate('someone sets it up', function (str) {
    console.log(str);
});
