'use strict';

const assert = require('assert');
const CachePolicy = require('..');

const req = {method:'GET', headers:{}};

describe('Response headers', function() {
    it('simple miss', function() {
        const cache = new CachePolicy(req, {headers:{}});
        assert(cache.stale());
    });

    it('simple hit', function() {
        const cache = new CachePolicy(req, {headers:{'cache-control': 'public, max-age=999999'}});
        assert(!cache.stale());
        assert.equal(cache.maxAge(), 999999);
    });

    it('weird syntax', function() {
        const cache = new CachePolicy(req, {headers:{'cache-control': ',,,,max-age =  456      ,'}});
        assert(!cache.stale());
        assert.equal(cache.maxAge(), 456);

        const cache2 = CachePolicy.fromObject(JSON.parse(JSON.stringify(cache.toObject())));
        assert(cache2 instanceof CachePolicy);
        assert(!cache2.stale());
        assert.equal(cache2.maxAge(), 456);
    });

    it('quoted syntax', function() {
        const cache = new CachePolicy(req, {headers:{'cache-control': '  max-age = "678"      '}});
        assert(!cache.stale());
        assert.equal(cache.maxAge(), 678);
    });

    it('IIS', function() {
        const cache = new CachePolicy(req, {headers:{'cache-control': 'private, public, max-age=259200'}}, {shared:false});
        assert(!cache.stale());
        assert.equal(cache.maxAge(), 259200);
    });

    it('pre-check tolerated', function() {
        const cc = 'pre-check=0, post-check=0, no-store, no-cache, max-age=100';
        const cache = new CachePolicy(req, {headers:{'cache-control': cc}});
        assert(cache.stale());
        assert(!cache.storable());
        assert.equal(cache.maxAge(), 0);
        assert.equal(cache.responseHeaders()['cache-control'], cc);
    });

    it('pre-check poison', function() {
        const origCC = 'pre-check=0, post-check=0, no-cache, no-store, max-age=100, custom, foo=bar';
        const res = {headers:{'cache-control': origCC, pragma: 'no-cache'}};
        const cache = new CachePolicy(req, res, {ignoreCargoCult:true});
        assert(!cache.stale());
        assert(cache.storable());
        assert.equal(cache.maxAge(), 100);

        const cc = cache.responseHeaders()['cache-control'];
        assert(!/pre-check/.test(cc), cc);
        assert(!/post-check/.test(cc), cc);
        assert(!/no-store/.test(cc), cc);

        assert(/max-age=100/.test(cc));
        assert(/custom(,|$)/.test(cc));
        assert(/foo=bar/.test(cc));

        assert.equal(res.headers['cache-control'], origCC);
        assert(res.headers['pragma']);
        assert(!cache.responseHeaders()['pragma']);
    });

    it('pre-check poison undefined header', function() {
        const origCC = 'pre-check=0, post-check=0, no-cache, no-store';
        const res = {headers:{'cache-control': origCC, expires: 'yesterday!'}};
        const cache = new CachePolicy(req, res, {ignoreCargoCult:true});
        assert(cache.stale());
        assert(cache.storable());
        assert.equal(cache.maxAge(), 0);

        const cc = cache.responseHeaders()['cache-control'];
        assert(!cc);

        assert(res.headers['expires']);
        assert(!cache.responseHeaders()['expires']);
    });

    it('cache with expires', function() {
        const cache = new CachePolicy(req, {headers:{
            'date': new Date().toGMTString(),
            'expires': new Date(Date.now() + 2000).toGMTString(),
        }});
        assert(!cache.stale());
        assert.equal(2, cache.maxAge());
    });

    it('cache expires no date', function() {
        const cache = new CachePolicy(req, {headers:{
            'cache-control': 'public',
            'expires': new Date(Date.now()+3600*1000).toGMTString(),
        }});
        assert(!cache.stale());
        assert(cache.maxAge() > 3595);
        assert(cache.maxAge() < 3605);
    });

    it('Ages', function() {
        let now = 1000;
        class TimeTravellingPolicy extends CachePolicy {
            now() {
                return now;
            }
        }
        const cache = new TimeTravellingPolicy(req, {headers:{
            'cache-control':'max-age=100',
            'age': '50',
        }});
        assert(cache.storable());

        assert.equal(50*1000, cache.timeToLive());
        assert(!cache.stale());
        now += 48*1000;
        assert.equal(2*1000, cache.timeToLive());
        assert(!cache.stale());
        now += 5*1000;
        assert(cache.stale());
        assert.equal(0, cache.timeToLive());
    });

    it('Age can make stale', function() {
        const cache = new CachePolicy(req, {headers:{
            'cache-control':'max-age=100',
            'age': '101',
        }});
        assert(cache.stale());
        assert(cache.storable());
    });

    it('Age not always stale', function() {
        const cache = new CachePolicy(req, {headers:{
            'cache-control':'max-age=20',
            'age': '15',
        }});
        assert(!cache.stale());
        assert(cache.storable());
    });

    it('Bogus age ignored', function() {
        const cache = new CachePolicy(req, {headers:{
            'cache-control':'max-age=20',
            'age': 'golden',
        }});
        assert(!cache.stale());
        assert(cache.storable());
    });

    it('cache old files', function() {
        const cache = new CachePolicy(req, {headers:{
            'date': new Date().toGMTString(),
            'last-modified': 'Mon, 07 Mar 2016 11:52:56 GMT',
        }});
        assert(!cache.stale());
        assert(cache.maxAge() > 100);
    });

    it('immutable simple hit', function() {
        const cache = new CachePolicy(req, {headers:{'cache-control': 'immutable, max-age=999999'}});
        assert(!cache.stale());
        assert.equal(cache.maxAge(), 999999);
    });

    it('immutable can expire', function() {
        const cache = new CachePolicy(req, {headers:{'cache-control': 'immutable, max-age=0'}});
        assert(cache.stale());
        assert.equal(cache.maxAge(), 0);
    });

    it('cache immutable files', function() {
        const cache = new CachePolicy(req, {headers:{
            'date': new Date().toGMTString(),
            'cache-control':'immutable',
            'last-modified': new Date().toGMTString(),
        }});
        assert(!cache.stale());
        assert(cache.maxAge() > 100);
    });

    it('immutable can be off', function() {
        const cache = new CachePolicy(req, {headers:{
            'date': new Date().toGMTString(),
            'cache-control':'immutable',
            'last-modified': new Date().toGMTString(),
        }}, {immutableMinTimeToLive: 0});
        assert(cache.stale());
        assert.equal(cache.maxAge(), 0);
    });

    it('pragma: no-cache', function() {
        const cache = new CachePolicy(req, {headers:{
            'pragma': 'no-cache',
            'last-modified': 'Mon, 07 Mar 2016 11:52:56 GMT',
        }});
        assert(cache.stale());
    });

    it('no-store', function() {
        const cache = new CachePolicy(req, {headers:{
            'cache-control': 'no-store, public, max-age=1',
        }});
        assert(cache.stale());
        assert.equal(0, cache.maxAge());
    });

    it('observe private cache', function() {
        const privateHeader = {
            'cache-control': 'private, max-age=1234',
        };
        const proxyCache = new CachePolicy(req, {headers:privateHeader});
        assert(proxyCache.stale());
        assert.equal(0, proxyCache.maxAge());

        const uaCache = new CachePolicy(req, {headers:privateHeader}, {shared:false});
        assert(!uaCache.stale());
        assert.equal(1234, uaCache.maxAge());
    });

    it('don\'t share cookies', function() {
        const cookieHeader = {
            'set-cookie': 'foo=bar',
            'cache-control': 'max-age=99',
        };
        const proxyCache = new CachePolicy(req, {headers:cookieHeader}, {shared:true});
        assert(proxyCache.stale());
        assert.equal(0, proxyCache.maxAge());

        const uaCache = new CachePolicy(req, {headers:cookieHeader}, {shared:false});
        assert(!uaCache.stale());
        assert.equal(99, uaCache.maxAge());
    });

    it('do share cookies if immutable', function() {
        const cookieHeader = {
            'set-cookie': 'foo=bar',
            'cache-control': 'immutable, max-age=99',
        };
        const proxyCache = new CachePolicy(req, {headers:cookieHeader}, {shared:true});
        assert(!proxyCache.stale());
        assert.equal(99, proxyCache.maxAge());
    });

    it('cache explicitly public cookie', function() {
        const cookieHeader = {
            'set-cookie': 'foo=bar',
            'cache-control': 'max-age=5, public',
        };
        const proxyCache = new CachePolicy(req, {headers:cookieHeader}, {shared:true});
        assert(!proxyCache.stale());
        assert.equal(5, proxyCache.maxAge());
    });

    it('miss max-age=0', function() {
        const cache = new CachePolicy(req, {headers:{
            'cache-control': 'public, max-age=0',
        }});
        assert(cache.stale());
        assert.equal(0, cache.maxAge());
    });

    it('uncacheable 503', function() {
        const cache = new CachePolicy(req, {
            status: 503,
            headers:{
                'cache-control': 'public, max-age=1000',
            }});
        assert(cache.stale());
        assert.equal(0, cache.maxAge());
    });

    it('cacheable 301', function() {
        const cache = new CachePolicy(req, {
            status: 301,
            headers:{
                'last-modified': 'Mon, 07 Mar 2016 11:52:56 GMT',
            }});
        assert(!cache.stale());
    });

    it('uncacheable 303', function() {
        const cache = new CachePolicy(req, {
            status: 303,
            headers:{
                'last-modified': 'Mon, 07 Mar 2016 11:52:56 GMT',
            }});
        assert(cache.stale());
        assert.equal(0, cache.maxAge());
    });

    it('cacheable 303', function() {
        const cache = new CachePolicy(req, {
            status: 303,
            headers:{
                'cache-control': 'max-age=1000',
            }});
        assert(!cache.stale());
    });

    it('uncacheable 412', function() {
        const cache = new CachePolicy(req, {
            status: 412,
            headers:{
                'cache-control': 'public, max-age=1000',
            }});
        assert(cache.stale());
        assert.equal(0, cache.maxAge());
    });

    it('expired expires cached with max-age', function() {
        const cache = new CachePolicy(req, {headers:{
            'cache-control': 'public, max-age=9999',
            'expires': 'Sat, 07 May 2016 15:35:18 GMT',
        }});
        assert(!cache.stale());
        assert.equal(9999, cache.maxAge());
    });

    it('expired expires cached with s-maxage', function() {
        const sMaxAgeHeaders = {
            'cache-control': 'public, s-maxage=9999',
            'expires': 'Sat, 07 May 2016 15:35:18 GMT',
        };
        const proxyCache = new CachePolicy(req, {headers:sMaxAgeHeaders});
        assert(!proxyCache.stale());
        assert.equal(9999, proxyCache.maxAge());

        const uaCache = new CachePolicy(req, {headers:sMaxAgeHeaders}, {shared:false});
        assert(uaCache.stale());
        assert.equal(0, uaCache.maxAge());
    });

    it('max-age wins over future expires', function() {
        const cache = new CachePolicy(req, {headers:{
            'cache-control': 'public, max-age=333',
            'expires': new Date(Date.now()+3600*1000).toGMTString(),
        }});
        assert(!cache.stale());
        assert.equal(333, cache.maxAge());
    });

    it('remove hop headers', function() {
        let now = 10000;
        class TimeTravellingPolicy extends CachePolicy {
            now() {
                return now;
            }
        }

        const res = {headers:{
            'te': 'deflate',
            'date': 'now',
            'custom': 'header',
            'oompa': 'lumpa',
            'connection': 'close, oompa, header',
            'age': '10',
            'cache-control': 'public, max-age=333',
        }};
        const cache = new TimeTravellingPolicy(req, res);

        now += 1005;
        const h = cache.responseHeaders();
        assert(!h.connection);
        assert(!h.te);
        assert(!h.oompa);
        assert.equal(h['cache-control'], 'public, max-age=333');
        assert.equal(h.date, 'now', "date must stay the same for expires, age, etc");
        assert.equal(h.custom, 'header');
        assert.equal(h.age, '11');
        assert.equal(res.headers.age, '10');

        const cache2 = TimeTravellingPolicy.fromObject(JSON.parse(JSON.stringify(cache.toObject())));
        assert(cache2 instanceof TimeTravellingPolicy);
        const h2 = cache2.responseHeaders();
        assert.deepEqual(h, h2);
    });
});
