import * as fixtures from '../../common/fixtures.mjs';
import { register } from 'node:module';

register(fixtures.fileURL('es-module-loaders', 'loader-load-passthru.mjs'));
