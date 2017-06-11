'use strict';

const assert = require('assert');
const CachePolicy = require('..');


describe('Other', function() {
    it('Thaw wrong object', function() {
        assert.throws(() => {
            CachePolicy.fromObject({});
        });
    });

    it('Missing headers', function() {
        assert.throws(() => {
            new CachePolicy({});
        });
        assert.throws(() => {
            new CachePolicy({headers:{}}, {});
        });

        const cache = new CachePolicy({headers:{}}, {headers:{}});
        assert.throws(() => {
            cache.satisfiesWithoutRevalidation({});
        });
        assert.throws(() => {
            cache.revalidatedPolicy({});
        });
        assert.throws(() => {
            cache.revalidatedPolicy({headers:{}}, {});
        });
    });
});
