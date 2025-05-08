import { Writable } from 'node:stream';
import { styleText } from 'node:util';

const streamTTY = new class extends Writable {
  isTTY = true;
}();
const streamNoTTY = new class extends Writable {
  isTTY = false;
}();

console.log(styleText('bgYellow', 'TTY', { stream: streamTTY }));
console.log(styleText('bgYellow', 'No TTY', { stream: streamNoTTY }));
console.log(styleText('bgYellow', 'stdout', { stream: process.stdout }));
console.log(styleText('bgYellow', 'stderr', { stream: process.stderr }));
