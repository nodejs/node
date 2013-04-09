// Load modules

var Url = require('url');
var Boom = require('boom');
var Cryptiles = require('cryptiles');
var Crypto = require('./crypto');
var Utils = require('./utils');


// Declare internals

var internals = {};


// Hawk authentication

/*
 * Arguments and options are the same as index.js with the exception that the only supported options are:
 * 'hostHeaderName', 'localtimeOffsetMsec'
 */

exports.authenticate = function (req, credentialsFunc, options, callback) {

    // Application time

    var now = Utils.now() + (options.localtimeOffsetMsec || 0);

    // Convert node Http request object to a request configuration object

    var request = Utils.parseRequest(req, options);
    if (request instanceof Error) {
        return callback(Boom.badRequest(request.message));
    }

    // Extract bewit

    //                                 1     2             3           4     
    var resource = request.url.match(/^(\/.*)([\?&])bewit\=([^&$]*)(?:&(.+))?$/);
    if (!resource) {
        return callback(Boom.unauthorized(null, 'Hawk'));
    }

    // Bewit not empty

    if (!resource[3]) {
        return callback(Boom.unauthorized('Empty bewit', 'Hawk'));
    }

    // Verify method is GET

    if (request.method !== 'GET' &&
        request.method !== 'HEAD') {

        return callback(Boom.unauthorized('Invalid method', 'Hawk'));
    }

    // No other authentication

    if (request.authorization) {
        return callback(Boom.badRequest('Multiple authentications', 'Hawk'));
    }

    // Parse bewit

    var bewitString = Utils.base64urlDecode(resource[3]);
    if (bewitString instanceof Error) {
        return callback(Boom.badRequest('Invalid bewit encoding'));
    }

    // Bewit format: id\exp\mac\ext ('\' is used because it is a reserved header attribute character)

    var bewitParts = bewitString.split('\\');
    if (!bewitParts ||
        bewitParts.length !== 4) {

        return callback(Boom.badRequest('Invalid bewit structure'));
    }

    var bewit = {
        id: bewitParts[0],
        exp: parseInt(bewitParts[1], 10),
        mac: bewitParts[2],
        ext: bewitParts[3] || ''
    };

    if (!bewit.id ||
        !bewit.exp ||
        !bewit.mac) {

        return callback(Boom.badRequest('Missing bewit attributes'));
    }

    // Construct URL without bewit

    var url = resource[1];
    if (resource[4]) {
        url += resource[2] + resource[4];
    }

    // Check expiration

    if (bewit.exp * 1000 <= now) {
        return callback(Boom.unauthorized('Access expired', 'Hawk'), null, bewit);
    }

    // Fetch Hawk credentials

    credentialsFunc(bewit.id, function (err, credentials) {

        if (err) {
            return callback(err, credentials || null, bewit.ext);
        }

        if (!credentials) {
            return callback(Boom.unauthorized('Unknown credentials', 'Hawk'), null, bewit);
        }

        if (!credentials.key ||
            !credentials.algorithm) {

            return callback(Boom.internal('Invalid credentials'), credentials, bewit);
        }

        if (Crypto.algorithms.indexOf(credentials.algorithm) === -1) {
            return callback(Boom.internal('Unknown algorithm'), credentials, bewit);
        }

        // Calculate MAC

        var mac = Crypto.calculateMac('bewit', {
            credentials: credentials,
            ts: bewit.exp,
            nonce: '',
            method: 'GET',
            resource: url,
            host: request.host,
            port: request.port,
            ext: bewit.ext
        });

        if (!Cryptiles.fixedTimeComparison(mac, bewit.mac)) {
            return callback(Boom.unauthorized('Bad mac', 'Hawk'), credentials, bewit);
        }

        // Successful authentication

        return callback(null, credentials, bewit);
    });
};


// Generate a bewit value for a given URI

/*
 * credentials is an object with the following keys: 'id, 'key', 'algorithm'.
 * options is an object with the following optional keys: 'ext', 'localtimeOffsetMsec'
 */
/*
    uri: 'http://example.com/resource?a=b' or object from Url.parse()
    options: {

        // Required

        credentials: {
            id: 'dh37fgj492je',
            key: 'aoijedoaijsdlaksjdl',
            algorithm: 'sha256'                             // 'sha1', 'sha256'
        },
        ttlSec: 60 * 60,                                    // TTL in seconds

        // Optional

        ext: 'application-specific',                        // Application specific data sent via the ext attribute
        localtimeOffsetMsec: 400                            // Time offset to sync with server time
    };
*/

exports.getBewit = function (uri, options) {

    // Validate inputs

    if (!uri ||
        (typeof uri !== 'string' && typeof uri !== 'object') ||
        !options ||
        typeof options !== 'object' ||
        !options.ttlSec) {

        return '';
    }

    options.ext = (options.ext === null || options.ext === undefined ? '' : options.ext);       // Zero is valid value

    // Application time

    var now = Utils.now() + (options.localtimeOffsetMsec || 0);

    // Validate credentials

    var credentials = options.credentials;
    if (!credentials ||
        !credentials.id ||
        !credentials.key ||
        !credentials.algorithm) {

        return '';
    }

    if (Crypto.algorithms.indexOf(credentials.algorithm) === -1) {
        return '';
    }

    // Parse URI

    if (typeof uri === 'string') {
        uri = Url.parse(uri);
    }

    // Calculate signature

    var exp = Math.floor(now / 1000) + options.ttlSec;
    var mac = Crypto.calculateMac('bewit', {
        credentials: credentials,
        ts: exp,
        nonce: '',
        method: 'GET',
        resource: uri.pathname + (uri.search || ''),                            // Maintain trailing '?'
        host: uri.hostname,
        port: uri.port || (uri.protocol === 'http:' ? 80 : 443),
        ext: options.ext
    });

    // Construct bewit: id\exp\mac\ext

    var bewit = credentials.id + '\\' + exp + '\\' + mac + '\\' + options.ext;
    return Utils.base64urlEncode(bewit);
};


