'use strict';

const assert = require('assert');
const CachePolicy = require('..');

const simpleRequest = {
    method:'GET',
    headers:{
      host:'www.w3c.org',
      connection: 'close',
    },
    url:'/Protocols/rfc2616/rfc2616-sec14.html',
};
function withHeaders(request, headers) {
    return Object.assign({}, request, {
        headers: Object.assign({}, request.headers, headers),
    });
}

const cacheableResponse = {headers:{'cache-control':'max-age=111'}};
const etaggedResponse = {headers:Object.assign({'etag':'"123456789"'},cacheableResponse.headers)};
const weakTaggedResponse = {headers:Object.assign({'etag':'W/"123456789"'},cacheableResponse.headers)};
const lastModifiedResponse = {headers:Object.assign({'last-modified':'Tue, 15 Nov 1994 12:45:26 GMT'},cacheableResponse.headers)};
const multiValidatorResponse = {headers:Object.assign({},etaggedResponse.headers,lastModifiedResponse.headers)};

function notModifiedResponseHeaders(firstRequest, firstResponse, secondRequest, secondResponse) {
    const cache = new CachePolicy(firstRequest, firstResponse);
    const headers = cache.revalidationHeaders(secondRequest);
    const {policy:newCache, modified} = cache.revalidatedPolicy({headers}, secondResponse);
    if (modified) {
        return false;
    }
    return newCache.responseHeaders();
}

function assertUpdates(firstRequest, firstResponse, secondRequest, secondResponse) {
    const headers = notModifiedResponseHeaders(firstRequest, withHeaders(firstResponse, {'foo': 'original', 'x-other':'original'}),
        secondRequest, withHeaders(secondResponse, {'foo': 'updated', 'x-ignore-new':'ignoreme'}));
    assert(headers);
    assert.equal(headers['foo'], 'updated');
    assert.equal(headers['x-other'], 'original');
    assert.strictEqual(headers['x-ignore-new'], undefined);
    assert.strictEqual(headers['etag'], secondResponse.headers.etag);
}

describe('Update revalidated', function() {
    it('Matching etags are updated', function(){
        assertUpdates(simpleRequest, etaggedResponse, simpleRequest, etaggedResponse);
    });

    it('Matching weak etags are updated', function(){
        assertUpdates(simpleRequest, weakTaggedResponse, simpleRequest, weakTaggedResponse);
    });

    it('Matching lastmod are updated', function(){
        assertUpdates(simpleRequest, lastModifiedResponse, simpleRequest, lastModifiedResponse);
    });

    it('Both matching are updated', function(){
        assertUpdates(simpleRequest, multiValidatorResponse, simpleRequest, multiValidatorResponse);
    });

    it('Checks status', function(){
        const response304 = Object.assign({}, multiValidatorResponse, {status:304});
        const response200 = Object.assign({}, multiValidatorResponse, {status:200});
        assertUpdates(simpleRequest, multiValidatorResponse, simpleRequest, response304);
        assert(!notModifiedResponseHeaders(simpleRequest, multiValidatorResponse, simpleRequest, response200));
    });

    it('Last-mod ignored if etag is wrong', function(){
        assert(!notModifiedResponseHeaders(simpleRequest, multiValidatorResponse, simpleRequest, withHeaders(multiValidatorResponse, {'etag':'bad'})));
        assert(!notModifiedResponseHeaders(simpleRequest, multiValidatorResponse, simpleRequest, withHeaders(multiValidatorResponse, {'etag':'W/bad'})));
    });

    it('Ignored if validator is missing', function(){
        assert(!notModifiedResponseHeaders(simpleRequest, etaggedResponse, simpleRequest, cacheableResponse));
        assert(!notModifiedResponseHeaders(simpleRequest, weakTaggedResponse, simpleRequest, cacheableResponse));
        assert(!notModifiedResponseHeaders(simpleRequest, lastModifiedResponse, simpleRequest, cacheableResponse));
    });

    it('Skips update of content-length', function(){
        const etaggedResponseWithLenght1 = withHeaders(etaggedResponse, {'content-length':1});
        const etaggedResponseWithLenght2 = withHeaders(etaggedResponse, {'content-length':2});
        const headers = notModifiedResponseHeaders(simpleRequest, etaggedResponseWithLenght1, simpleRequest, etaggedResponseWithLenght2);
        assert.equal(1, headers['content-length']);
    });

    it('Ignored if validator is different', function(){
        assert(!notModifiedResponseHeaders(simpleRequest, lastModifiedResponse, simpleRequest, etaggedResponse));
        assert(!notModifiedResponseHeaders(simpleRequest, lastModifiedResponse, simpleRequest, weakTaggedResponse));
        assert(!notModifiedResponseHeaders(simpleRequest, etaggedResponse, simpleRequest, lastModifiedResponse));
    });

    it('Ignored if validator doesn\'t match', function(){
        assert(!notModifiedResponseHeaders(simpleRequest, etaggedResponse, simpleRequest, withHeaders(etaggedResponse, {etag:'"other"'})), "bad etag");
        assert(!notModifiedResponseHeaders(simpleRequest, lastModifiedResponse, simpleRequest, withHeaders(lastModifiedResponse, {'last-modified':'dunno'})), "bad lastmod");
    });
});
