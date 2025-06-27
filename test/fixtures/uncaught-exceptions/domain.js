const domain = require('domain');

var d = domain.create();
d.on('error', function(err) {
  console.log('[ignored]', err);
});

d.run(function() {
  setImmediate(function() {
    throw new Error('in domain');
  });
});
