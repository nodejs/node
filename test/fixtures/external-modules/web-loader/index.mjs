import { register } from 'node:module';

register('./ts.mjs', import.meta.url);
register('./txt.mjs', import.meta.url);
