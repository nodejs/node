import { test } from 'node:test';
import { strictEqual } from 'node:assert';
import internal from 'internal/options';

test('it should not receive --config-file option', () => {
    const optionValue = internal.getOptionValue("--config-file");
    strictEqual(optionValue, '');
})
