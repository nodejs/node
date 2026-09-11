'use strict';

import { spawnSync } from 'child_process';
import { strictEqual, match } from "node:assert";

const result = spawnSync(process.execPath, ['--watch'], { encoding: 'utf8' });

strictEqual(result.status, 9);
match(result.stderr, /--watch requires specifying a file/)
