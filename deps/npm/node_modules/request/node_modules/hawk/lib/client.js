// Load modules

var Url = require('url');
var Hoek = require('hoek');
var Cryptiles = require('cryptiles');
var Crypto = require('./crypto');
var Utils = require('./utils');


// Declare internals

var internals = {};


// Generate an Authorization header for a given request

/*
    uri: 'http://example.com/resource?a=b' or object from Url.parse()
    method: HTTP verb (e.g. 'GET', 'POST')
    options: {

        // Required

        credentials: {
            id: 'dh37fgj492je',
            key: 'aoijedoaijsdlaksjdl',
            algorithm: 'sha256'                                 // 'sha1', 'sha256'
        },

        // Optional

        ext: 'application-specific',                        // Application specific data sent via the ext attribute
        timestamp: Date.now(),                              // A pre-calculated timestamp
        nonce: '2334f34f',                                  // A pre-generated nonce
        localtimeOffsetMsec: 400,                           // Time offset to sync with server time (ignored if timestamp provided)
        payload: '{"some":"payload"}',                      // UTF-8 encoded string for body hash generation (ignored if hash provided)
        contentType: 'application/json',                    // Payload content-type (ignored if hash provided)
        hash: 'U4MKKSmiVxk37JCCrAVIjV=',                    // Pre-calculated payload hash
        app: '24s23423f34dx',                               // Oz application id
        dlg: '234sz34tww3sd'                                // Oz delegated-by application id
    }
*/

exports.header = function (uri, method, options) {

    var result = {
        field: '',
        artifacts: {}
    };

    // Validate inputs

    if (!uri || (typeof uri !== 'string' && typeof uri !== 'object') ||
        !method || typeof method !== 'string' ||
        !options || typeof options !== 'object') {

        return result;
    }

    // Application time

    var timestamp = options.timestamp || Math.floor((Utils.now() + (options.localtimeOffsetMsec || 0)) / 1000)

    // Validate credentials

    var credentials = options.credentials;
    if (!credentials ||
        !credentials.id ||
        !credentials.key ||
        !credentials.algorithm) {

        // Invalid credential object
        return result;
    }

    if (Crypto.algorithms.indexOf(credentials.algorithm) === -1) {
        return result;
    }

    // Parse URI

    if (typeof uri === 'string') {
        uri = Url.parse(uri);
    }

    // Calculate signature

    var artifacts = {
        credentials: credentials,
        ts: timestamp,
        nonce: options.nonce || Cryptiles.randomString(6),
        method: method,
        resource: uri.pathname + (uri.search || ''),                            // Maintain trailing '?'
        host: uri.hostname,
        port: uri.port || (uri.protocol === 'http:' ? 80 : 443),
        hash: options.hash,
        ext: options.ext,
        app: options.app,
        dlg: options.dlg
    };

    result.artifacts = artifacts;

    // Calculate payload hash

    if (!artifacts.hash &&
        options.hasOwnProperty('payload')) {

        artifacts.hash = Crypto.calculateHash(options.payload, credentials.algorithm, options.contentType);
    }

    var mac = Crypto.calculateMac('header', artifacts);

    // Construct header

    var hasExt = artifacts.ext !== null && artifacts.ext !== undefined && artifacts.ext !== '';       // Other falsey values allowed
    var header = 'Hawk id="' + credentials.id +
                 '", ts="' + artifacts.ts +
                 '", nonce="' + artifacts.nonce +
                 (artifacts.hash ? '", hash="' + artifacts.hash : '') +
                 (hasExt ? '", ext="' + Utils.escapeHeaderAttribute(artifacts.ext) : '') +
                 '", mac="' + mac + '"';

    if (artifacts.app) {
        header += ', app="' + artifacts.app +
                  (artifacts.dlg ? '", dlg="' + artifacts.dlg : '') + '"';
    }

    result.field = header;

    return result;
};


// Validate server response

/*
    res:        node's response object
    artifacts:  object recieved from header().artifacts
    options: {
        payload:    optional payload received
        required:   specifies if a Server-Authorization header is required. Defaults to 'false'
    }
*/

exports.authenticate = function (res, artifacts, options) {

    artifacts = Hoek.clone(artifacts);
    options = options || {};

    if (res.headers['www-authenticate']) {

        // Parse HTTP WWW-Authenticate header

        var attributes = Utils.parseAuthorizationHeader(res.headers['www-authenticate'], ['ts', 'tsm', 'error']);
        if (attributes instanceof Error) {
            return false;
        }

        if (attributes.ts) {
            var tsm = Crypto.calculateTsMac(attributes.ts, artifacts.credentials);
            if (!Cryptiles.fixedTimeComparison(tsm, attributes.tsm)) {
                return false;
            }
        }
    }

    // Parse HTTP Server-Authorization header

    if (!res.headers['server-authorization'] &&
        !options.required) {

        return true;
    }

    var attributes = Utils.parseAuthorizationHeader(res.headers['server-authorization'], ['mac', 'ext', 'hash']);
    if (attributes instanceof Error) {
        return false;
    }

    artifacts.ext = attributes.ext;
    artifacts.hash = attributes.hash;

    var mac = Crypto.calculateMac('response', artifacts);
    if (!Cryptiles.fixedTimeComparison(mac, attributes.mac)) {
        return false;
    }

    if (!options.hasOwnProperty('payload')) {
        return true;
    }

    if (!attributes.hash) {
        return false;
    }

    var calculatedHash = Crypto.calculateHash(options.payload, artifacts.credentials.algorithm, res.headers['content-type']);
    return Cryptiles.fixedTimeComparison(calculatedHash, attributes.hash);
};

