import { register } from 'node:module';

register('./hooks.mjs', import.meta.url);

const { default: value } = await import('pkg');
postMessage(value);
