import { mustCall } from '../common/index.mjs';
import { fork } from 'child_process';

if (process.argv[2] === 'child') {
  process.disconnect();
} else {
  const child = fork(new URL(import.meta.url), ['child']);

  child.on('disconnect', mustCall());
  child.once('exit', mustCall());
}
