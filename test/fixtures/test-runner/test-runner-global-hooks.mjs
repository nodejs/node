import { run } from 'node:test';
import { spec } from 'node:test/reporters';
import { parseArgs } from 'node:util';

const options = {
    file: {
        type: 'string',
    },
    globalSetup: {
        type: 'string',
    },
    isolation: {
        type: 'string',
    },
};

const {
    values,
} = parseArgs({ args: process.argv.slice(2), options });

let files;
let globalSetupPath;

if (values.file) {
    files = [values.file];
}

if (values.globalSetup) {
    globalSetupPath = values.globalSetup;
}

run({
    files,
    globalSetupPath,
}).compose(spec).pipe(process.stdout);
