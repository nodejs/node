import { test } from 'node:test';
import { strictEqual } from 'node:assert';
import internal from 'internal/options';

test('it should receive --experimental-config-file option', () => {
    const optionValue = internal.getOptionValue("--experimental-config-file");
    strictEqual(optionValue, 'node.config.json');
})
