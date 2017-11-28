// Flags: --experimental-modules
/* eslint-disable required-modules */

import assert from 'assert';
import { enum as e } from
  '../fixtures/es-module-loaders/reserved-keywords.js';

assert(e);
