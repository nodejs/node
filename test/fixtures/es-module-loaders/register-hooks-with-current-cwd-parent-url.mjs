import { register } from 'node:module';
import { pathToFileURL } from 'node:url';
register('./hooks-initialize.mjs', pathToFileURL('./'));
