'use strict';

const assert = require('assert');
const CachePolicy = require('..');

const publicCacheableResponse = {headers:{'cache-control': 'public, max-age=222'}};
const cacheableResponse = {headers:{'cache-control': 'max-age=111'}};

describe('Request properties', function() {
    it('No store kills cache', function() {
        const cache = new CachePolicy({method:'GET',headers:{'cache-control':'no-store'}}, publicCacheableResponse);
        assert(cache.stale());
        assert(!cache.storable());
    });

    it('POST not cacheable by default', function() {
        const cache = new CachePolicy({method:'POST',headers:{}}, {headers:{'cache-control': 'public'}});
        assert(cache.stale());
        assert(!cache.storable());
    });

    it('POST cacheable explicitly', function() {
        const cache = new CachePolicy({method:'POST',headers:{}}, publicCacheableResponse);
        assert(!cache.stale());
        assert(cache.storable());
    });

    it('Public cacheable auth is OK', function() {
        const cache = new CachePolicy({method:'GET',headers:{'authorization': 'test'}}, publicCacheableResponse);
        assert(!cache.stale());
        assert(cache.storable());
    });

    it('Proxy cacheable auth is OK', function() {
        const cache = new CachePolicy({method:'GET',headers:{'authorization': 'test'}}, {headers:{'cache-control':'max-age=0,s-maxage=12'}});
        assert(!cache.stale());
        assert(cache.storable());

        const cache2 = CachePolicy.fromObject(JSON.parse(JSON.stringify(cache.toObject())));
        assert(cache2 instanceof CachePolicy);
        assert(!cache2.stale());
        assert(cache2.storable());
    });

    it('Private auth is OK', function() {
        const cache = new CachePolicy({method:'GET',headers:{'authorization': 'test'}}, cacheableResponse, {shared:false});
        assert(!cache.stale());
        assert(cache.storable());
    });

    it('Revalidated auth is OK', function() {
        const cache = new CachePolicy({headers:{'authorization': 'test'}}, {headers:{'cache-control':'max-age=88,must-revalidate'}});
        assert(cache.storable());
    });

    it('Auth prevents caching by default', function() {
        const cache = new CachePolicy({method:'GET',headers:{'authorization': 'test'}}, cacheableResponse);
        assert(cache.stale());
        assert(!cache.storable());
    });
});
