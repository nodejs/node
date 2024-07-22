'use strict';

require('../../../common');
const assert = require('assert');
const child_process = require('child_process');

async function spawnNodeRunner(path) {
    return new Promise((resolve, reject) => {
        child_process.exec(`node ${path}`, (err, stdout, stderr) => {
            if (err) {
                reject(err);
            } else {
                resolve(stdout);
            }
        });
    });
};

async function main() {
    assert.strictEqual((await spawnNodeRunner('.')).trim(), require('.').trim());
}

main();