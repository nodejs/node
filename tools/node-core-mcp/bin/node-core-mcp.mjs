#!/usr/bin/env node
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { server, start } from '../lib/server.mjs';
import { registerTools } from '../lib/tools.mjs';

const argv = process.argv.slice(2);
const repoIdx = argv.indexOf('--repo');
const ROOT = resolve(repoIdx !== -1 ? argv[repoIdx + 1] : dirname(fileURLToPath(import.meta.url)) + '/../../..');

registerTools(server, ROOT);
start();
