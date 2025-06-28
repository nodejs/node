import { test } from 'node:test';
import { strictEqual } from 'node:assert';
import internal from 'internal/options';

test('it should not receive --experimental-config-file option', () => {
    const optionValue = internal.getOptionValue("--experimental-config-file");
    strictEqual(optionValue, '');
})
