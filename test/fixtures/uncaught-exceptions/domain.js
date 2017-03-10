const domain = require('domain');

const d = domain.create();
d.on('error', (err) => {
  console.log('[ignored]', err.stack);
});

d.run(() => {
  setImmediate(() => {
    throw new Error('in domain');
  });
});
