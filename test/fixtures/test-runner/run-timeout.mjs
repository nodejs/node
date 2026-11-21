import test, { after } from 'node:test';

// Keep the event loop alive until both tests time out.
const interval = setInterval(() => {}, 1000);
after(() => clearInterval(interval));

test('pending promise', () => new Promise(() => {}));

test('pending assertion plan', (t) => {
  t.plan(1, { wait: true });
});
