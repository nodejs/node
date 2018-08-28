// Flags: --experimental-modules -m
/* eslint-disable node-core/required-modules */
import { message } from '../fixtures/es-modules/message.mjs';
import assert from 'assert';

assert.strictEqual(message, 'A message');
