import assert from 'node:assert/strict';
import { before, describe, it, mock } from 'node:test';

describe('foo', { concurrency: true }, () => {
    let barMock = mock.fn();
    let foo;

    before(async () => {
        const barNamedExports = await import('../coverage/bar.mts')
            .then(({ default: _, ...rest }) => rest);

        mock.module('../coverage/bar.mts', {
            defaultExport: barMock,
            namedExports: barNamedExports,
        });

        ({ foo } = await import('../coverage/foo.mts'));
    });

    it('should do the thing', () => {
        barMock.mock.mockImplementationOnce(() => 42);

        assert.equal(foo(), 42);
    });
});
