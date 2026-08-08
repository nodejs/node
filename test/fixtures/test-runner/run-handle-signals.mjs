import { run } from 'node:test';
import { tap } from 'node:test/reporters';

const mode = process.argv[2];
const testFile = process.argv[3];

let handleSignals;

if (mode === 'handle-signals') {
  handleSignals = true;
} else if (mode === 'no-handle-signals') {
  handleSignals = false;
}

if (mode === 'default' || mode === 'no-handle-signals') {
  process.on('SIGINT', () => {
    process.stdout.write('user SIGINT handler\n', () => {
      process.exit(0);
    });
  });
}

const stream = run({
  files: [testFile],
  handleSignals,
  isolation: 'none',
});

stream.once('test:dequeue', () => {
  console.log('READY');
});

stream.compose(tap).pipe(process.stdout);
