import { register } from 'node:module';
import fixtures from '../../common/fixtures.js';

register(fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'));
