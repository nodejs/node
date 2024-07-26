import { register } from 'node:module';
import { MessageChannel } from 'node:worker_threads';

const { port1, port2 } = new MessageChannel();

port1.unref();
port2.unref();

register((new URL('./hooks.mjs', import.meta.url)).href);
