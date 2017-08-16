var JSONStream = require('../');
var test = require('tape')

test('#66', function (t) {
   var error = 0;
   var stream = JSONStream
    .parse()
    .on('error', function (err) {
        t.ok(err);
        error++;
    })
    .on('end', function () {
        t.ok(error === 1);
        t.end();
    });

    stream.write('["foo":bar[');
    stream.end();

});

test('#81 - failure to parse nested objects', function (t) {
  var stream = JSONStream
    .parse('.bar.foo')
    .on('error', function (err) {
      t.error(err);
    })
    .on('end', function () {
      t.end();
    });

  stream.write('{"bar":{"foo":"baz"}}');
  stream.end();
});
