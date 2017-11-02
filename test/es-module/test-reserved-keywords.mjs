// Flags: --experimental-modules
/* eslint-disable required-modules */

import assert from 'assert';
import { delete as d } from
  '../fixtures/es-module-loaders/reserved-keywords.js';

assert(d);
