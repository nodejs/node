import {MessageChannel} from 'node:worker_threads';
import {register} from 'node:module';
import {once} from 'node:events';
import fixtures from '../../common/fixtures.js';

const {port1, port2} = new MessageChannel();
port1.on('message', (msg) => {
  console.log('message', msg);
});
const result = register(
  fixtures.fileURL('es-module-loaders/hooks-initialize-port.mjs'),
  {data: port2, transferList: [port2]},
);
console.log('register', result);

const timeout = setTimeout(() => {}, 2**31 - 1); // to keep the process alive.
await Promise.all([
  once(port1, 'message').then(() => once(port1, 'message')),
  import('node:os'),
]);
clearTimeout(timeout);
port1.close();
