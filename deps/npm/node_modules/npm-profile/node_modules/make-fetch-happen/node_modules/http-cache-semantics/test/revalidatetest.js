'use strict';

const assert = require('assert');
const CachePolicy = require('..');

const simpleRequest = {
    method:'GET',
    headers:{
      host:'www.w3c.org',
      connection: 'close',
      'x-custom': 'yes',
    },
    url:'/Protocols/rfc2616/rfc2616-sec14.html',
};
function simpleRequestBut(overrides) {
    return Object.assign({}, simpleRequest, overrides);
}

const cacheableResponse = {headers:{'cache-control':'max-age=111'}};
const etaggedResponse = {headers:Object.assign({'etag':'"123456789"'},cacheableResponse.headers)};
const lastModifiedResponse = {headers:Object.assign({'last-modified':'Tue, 15 Nov 1994 12:45:26 GMT'},cacheableResponse.headers)};
const multiValidatorResponse = {headers:Object.assign({},etaggedResponse.headers,lastModifiedResponse.headers)};
const alwaysVariableResponse = {headers:Object.assign({'vary':'*'},cacheableResponse.headers)};

function assertHeadersPassed(headers) {
     assert.strictEqual(headers.connection, undefined);
     assert.strictEqual(headers['x-custom'], 'yes');
}
function assertNoValidators(headers) {
     assert.strictEqual(headers['if-none-match'], undefined);
     assert.strictEqual(headers['if-modified-since'], undefined);
}

describe('Can be revalidated?', function() {
    it('ok if method changes to HEAD', function(){
         const cache = new CachePolicy(simpleRequest,etaggedResponse);
         const headers = cache.revalidationHeaders(simpleRequestBut({method:'HEAD'}));
         assertHeadersPassed(headers);
         assert.equal(headers['if-none-match'], '"123456789"');
    });

    it('not if method mismatch (other than HEAD)', function(){
        const cache = new CachePolicy(simpleRequest,etaggedResponse);
        const incomingRequest = simpleRequestBut({method:'POST'});
        const headers = cache.revalidationHeaders(incomingRequest);
        assertHeadersPassed(headers);
        assertNoValidators(headers);
    });

    it('not if url mismatch', function(){
        const cache = new CachePolicy(simpleRequest,etaggedResponse);
        const incomingRequest = simpleRequestBut({url:'/yomomma'});
        const headers = cache.revalidationHeaders(incomingRequest);
        assertHeadersPassed(headers);
        assertNoValidators(headers);
    });

    it('not if host mismatch', function(){
        const cache = new CachePolicy(simpleRequest,etaggedResponse);
        const incomingRequest = simpleRequestBut({headers:{host:'www.w4c.org'}});
        const headers = cache.revalidationHeaders(incomingRequest);
        assertNoValidators(headers);
        assert.strictEqual(headers['x-custom'], undefined);
    });

    it('not if vary fields prevent', function(){
        const cache = new CachePolicy(simpleRequest,alwaysVariableResponse);
        const headers = cache.revalidationHeaders(simpleRequest);
        assertHeadersPassed(headers);
        assertNoValidators(headers);
    });

    it('when entity tag validator is present', function() {
        const cache = new CachePolicy(simpleRequest, etaggedResponse);
        const headers = cache.revalidationHeaders(simpleRequest);
        assertHeadersPassed(headers);
        assert.equal(headers['if-none-match'], '"123456789"');
    });

    it('skips weak validtors on post', function() {
        const postReq = simpleRequestBut({method:'POST', headers:{'if-none-match': 'W/"weak", "strong", W/"weak2"'}});
        const cache = new CachePolicy(postReq, multiValidatorResponse);
        const headers = cache.revalidationHeaders(postReq);
        assert.equal(headers['if-none-match'], '"strong", "123456789"');
        assert.strictEqual(undefined, headers['if-modified-since']);
    });

    it('skips weak validtors on post 2', function() {
        const postReq = simpleRequestBut({method:'POST', headers:{'if-none-match': 'W/"weak"'}});
        const cache = new CachePolicy(postReq, lastModifiedResponse);
        const headers = cache.revalidationHeaders(postReq);
        assert.strictEqual(undefined, headers['if-none-match']);
        assert.strictEqual(undefined, headers['if-modified-since']);
    });

    it('merges validtors', function() {
        const postReq = simpleRequestBut({headers:{'if-none-match': 'W/"weak", "strong", W/"weak2"'}});
        const cache = new CachePolicy(postReq, multiValidatorResponse);
        const headers = cache.revalidationHeaders(postReq);
        assert.equal(headers['if-none-match'], 'W/"weak", "strong", W/"weak2", "123456789"');
        assert.equal('Tue, 15 Nov 1994 12:45:26 GMT', headers['if-modified-since']);
    });

    it('when last-modified validator is present', function() {
        const cache = new CachePolicy(simpleRequest, lastModifiedResponse);
        const headers = cache.revalidationHeaders(simpleRequest);
        assertHeadersPassed(headers);
        assert.equal(headers['if-modified-since'], 'Tue, 15 Nov 1994 12:45:26 GMT');
    });

    it('not without validators', function() {
        const cache = new CachePolicy(simpleRequest, cacheableResponse);
        const headers = cache.revalidationHeaders(simpleRequest);
        assertHeadersPassed(headers);
        assertNoValidators(headers);
    })

});

describe('Validation request', function(){
    it('removes warnings', function() {
      const cache = new CachePolicy({headers:{}}, {headers:{
          "warning": "199 test danger",
      }});

      assert.strictEqual(undefined, cache.responseHeaders().warning);
    });

    it('must contain any etag', function(){
        const cache = new CachePolicy(simpleRequest,multiValidatorResponse);
        const expected = multiValidatorResponse.headers.etag;
        const actual = cache.revalidationHeaders(simpleRequest)['if-none-match'];
        assert.equal(actual,expected);
    });

    it('merges etags', function(){
        const cache = new CachePolicy(simpleRequest, etaggedResponse);
        const expected = `"foo", "bar", ${etaggedResponse.headers.etag}`;
        const headers = cache.revalidationHeaders(simpleRequestBut({headers:{
            host:'www.w3c.org',
            'if-none-match': '"foo", "bar"',
        }}));
        assert.equal(headers['if-none-match'],expected);
    });

    it('should send the Last-Modified value', function(){
        const cache = new CachePolicy(simpleRequest,multiValidatorResponse);
        const expected = multiValidatorResponse.headers['last-modified'];
        const actual = cache.revalidationHeaders(simpleRequest)['if-modified-since'];
        assert.equal(actual,expected);
    });

    it('should not send the Last-Modified value for POST', function(){
        const postReq = {method:'POST', headers:{'if-modified-since':'yesterday'}};
        const cache = new CachePolicy(postReq, lastModifiedResponse);
        const actual = cache.revalidationHeaders(postReq)['if-modified-since'];
        assert.equal(actual, undefined);
    });

    it('should not send the Last-Modified value for range requests', function(){
        const rangeReq = {method:'GET', headers:{'accept-ranges':'1-3', 'if-modified-since':'yesterday'}};
        const cache = new CachePolicy(rangeReq, lastModifiedResponse);
        const actual = cache.revalidationHeaders(rangeReq)['if-modified-since'];
        assert.equal(actual, undefined);
    });
});
