var qrcode = require('../lib/main'),
    url = 'https://google.com/';

qrcode.generate(url, { small: true }, function (qr) {
    console.log(qr);
});
