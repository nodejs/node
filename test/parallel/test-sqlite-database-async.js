import { isWindows, skipIfSQLiteMissing } from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
skipIfSQLiteMissing();
const { Database } = await import('node:sqlite');

const asyncDb = new Database(':memory:');
asyncDb.exec('CREATE TABLE data (key INTEGER PRIMARY KEY, value TEXT) STRICT;');
