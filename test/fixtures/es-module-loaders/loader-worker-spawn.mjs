import { Worker } from 'worker_threads';
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const fixtures = require('../../common/fixtures.js');

new Worker(fixtures.path('empty.js')).unref();
