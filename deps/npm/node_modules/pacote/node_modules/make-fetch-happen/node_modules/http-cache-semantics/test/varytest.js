'use strict';

const assert = require('assert');
const CachePolicy = require('..');

describe('Vary', function() {
    it('Basic', function() {
        const policy = new CachePolicy({headers:{'weather': 'nice'}}, {headers:{'cache-control':'max-age=5','vary':'weather'}});

        assert(policy.satisfiesWithoutRevalidation({headers:{'weather': 'nice'}}));
        assert(!policy.satisfiesWithoutRevalidation({headers:{'weather': 'bad'}}));
    });

    it("* doesn't match", function() {
        const policy = new CachePolicy({headers:{'weather': 'ok'}}, {headers:{'cache-control':'max-age=5','vary':'*'}});

        assert(!policy.satisfiesWithoutRevalidation({headers:{'weather': 'ok'}}));
    });

    it("* is stale", function() {
        const policy1 = new CachePolicy({headers:{'weather': 'ok'}}, {headers:{'cache-control':'public,max-age=99', 'vary':'*'}});
        const policy2 = new CachePolicy({headers:{'weather': 'ok'}}, {headers:{'cache-control':'public,max-age=99', 'vary':'weather'}});

        assert(policy1.stale());
        assert(!policy2.stale());
    });

    it('Values are case-sensitive', function() {
        const policy = new CachePolicy({headers:{'weather': 'BAD'}}, {headers:{'cache-control':'max-age=5','vary':'Weather'}});

        assert(policy.satisfiesWithoutRevalidation({headers:{'weather': 'BAD'}}));
        assert(!policy.satisfiesWithoutRevalidation({headers:{'weather': 'bad'}}));
    });

    it('Irrelevant headers ignored', function() {
        const policy = new CachePolicy({headers:{'weather': 'nice'}}, {headers:{'cache-control':'max-age=5','vary':'moon-phase'}});

        assert(policy.satisfiesWithoutRevalidation({headers:{'weather': 'bad'}}));
        assert(policy.satisfiesWithoutRevalidation({headers:{'sun': 'shining'}}));
        assert(!policy.satisfiesWithoutRevalidation({headers:{'moon-phase': 'full'}}));
    });

    it('Absence is meaningful', function() {
        const policy = new CachePolicy({headers:{'weather': 'nice'}}, {headers:{'cache-control':'max-age=5','vary':'moon-phase, weather'}});

        assert(policy.satisfiesWithoutRevalidation({headers:{'weather': 'nice'}}));
        assert(!policy.satisfiesWithoutRevalidation({headers:{'weather': 'nice', 'moon-phase': ''}}));
        assert(!policy.satisfiesWithoutRevalidation({headers:{}}));
    });

    it('All values must match', function() {
        const policy = new CachePolicy({headers:{'sun': 'shining', 'weather': 'nice'}}, {headers:{'cache-control':'max-age=5','vary':'weather, sun'}});

        assert(policy.satisfiesWithoutRevalidation({headers:{'sun': 'shining', 'weather': 'nice'}}));
        assert(!policy.satisfiesWithoutRevalidation({headers:{'sun': 'shining', 'weather': 'bad'}}));
    });

    it('Whitespace is OK', function() {
        const policy = new CachePolicy({headers:{'sun': 'shining', 'weather': 'nice'}}, {headers:{'cache-control':'max-age=5','vary':'    weather       ,     sun     '}});

        assert(policy.satisfiesWithoutRevalidation({headers:{'sun': 'shining', 'weather': 'nice'}}));
        assert(!policy.satisfiesWithoutRevalidation({headers:{'weather': 'nice'}}));
        assert(!policy.satisfiesWithoutRevalidation({headers:{'sun': 'shining'}}));
    });

    it('Order is irrelevant', function() {
        const policy1 = new CachePolicy({headers:{'sun': 'shining', 'weather': 'nice'}}, {headers:{'cache-control':'max-age=5','vary':'weather, sun'}});
        const policy2 = new CachePolicy({headers:{'sun': 'shining', 'weather': 'nice'}}, {headers:{'cache-control':'max-age=5','vary':'sun, weather'}});

        assert(policy1.satisfiesWithoutRevalidation({headers:{'weather': 'nice', 'sun': 'shining'}}));
        assert(policy1.satisfiesWithoutRevalidation({headers:{'sun': 'shining', 'weather': 'nice'}}));
        assert(policy2.satisfiesWithoutRevalidation({headers:{'weather': 'nice', 'sun': 'shining'}}));
        assert(policy2.satisfiesWithoutRevalidation({headers:{'sun': 'shining', 'weather': 'nice'}}));
    });
});
