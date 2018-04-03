'use strict';

// Load modules

const Hoek = require('hoek');


// Declare internals

const internals = {
    STATUS_CODES: Object.setPrototypeOf({
        '100': 'Continue',
        '101': 'Switching Protocols',
        '102': 'Processing',
        '200': 'OK',
        '201': 'Created',
        '202': 'Accepted',
        '203': 'Non-Authoritative Information',
        '204': 'No Content',
        '205': 'Reset Content',
        '206': 'Partial Content',
        '207': 'Multi-Status',
        '300': 'Multiple Choices',
        '301': 'Moved Permanently',
        '302': 'Moved Temporarily',
        '303': 'See Other',
        '304': 'Not Modified',
        '305': 'Use Proxy',
        '307': 'Temporary Redirect',
        '400': 'Bad Request',
        '401': 'Unauthorized',
        '402': 'Payment Required',
        '403': 'Forbidden',
        '404': 'Not Found',
        '405': 'Method Not Allowed',
        '406': 'Not Acceptable',
        '407': 'Proxy Authentication Required',
        '408': 'Request Time-out',
        '409': 'Conflict',
        '410': 'Gone',
        '411': 'Length Required',
        '412': 'Precondition Failed',
        '413': 'Request Entity Too Large',
        '414': 'Request-URI Too Large',
        '415': 'Unsupported Media Type',
        '416': 'Requested Range Not Satisfiable',
        '417': 'Expectation Failed',
        '418': 'I\'m a teapot',
        '422': 'Unprocessable Entity',
        '423': 'Locked',
        '424': 'Failed Dependency',
        '425': 'Unordered Collection',
        '426': 'Upgrade Required',
        '428': 'Precondition Required',
        '429': 'Too Many Requests',
        '431': 'Request Header Fields Too Large',
        '451': 'Unavailable For Legal Reasons',
        '500': 'Internal Server Error',
        '501': 'Not Implemented',
        '502': 'Bad Gateway',
        '503': 'Service Unavailable',
        '504': 'Gateway Time-out',
        '505': 'HTTP Version Not Supported',
        '506': 'Variant Also Negotiates',
        '507': 'Insufficient Storage',
        '509': 'Bandwidth Limit Exceeded',
        '510': 'Not Extended',
        '511': 'Network Authentication Required'
    }, null)
};


exports.boomify = function (error, options) {

    Hoek.assert(error instanceof Error, 'Cannot wrap non-Error object');

    options = options || {};

    if (!error.isBoom) {
        return internals.initialize(error, options.statusCode || 500, options.message);
    }

    if (options.override === false ||                           // Defaults to true
        (!options.statusCode && !options.message)) {

        return error;
    }

    return internals.initialize(error, options.statusCode || error.output.statusCode, options.message);
};


exports.wrap = function (error, statusCode, message) {

    Hoek.assert(error instanceof Error, 'Cannot wrap non-Error object');
    Hoek.assert(!error.isBoom || (!statusCode && !message), 'Cannot provide statusCode or message with boom error');

    return (error.isBoom ? error : internals.initialize(error, statusCode || 500, message));
};


exports.create = function (statusCode, message, data) {

    return internals.create(statusCode, message, data, exports.create);
};


internals.create = function (statusCode, message, data, ctor) {

    if (message instanceof Error) {
        if (data) {
            message.data = data;
        }

        return exports.wrap(message, statusCode);
    }

    const error = new Error(message ? message : undefined);         // Avoids settings null message
    Error.captureStackTrace(error, ctor);                           // Filter the stack to our external API
    error.data = data || null;
    internals.initialize(error, statusCode);
    error.typeof = ctor;

    return error;
};


internals.initialize = function (error, statusCode, message) {

    const numberCode = parseInt(statusCode, 10);
    Hoek.assert(!isNaN(numberCode) && numberCode >= 400, 'First argument must be a number (400+):', statusCode);

    error.isBoom = true;
    error.isServer = numberCode >= 500;

    if (!error.hasOwnProperty('data')) {
        error.data = null;
    }

    error.output = {
        statusCode: numberCode,
        payload: {},
        headers: {}
    };

    error.reformat = internals.reformat;

    if (!message &&
        !error.message) {

        error.reformat();
        message = error.output.payload.error;
    }

    if (message) {
        error.message = (message + (error.message ? ': ' + error.message : ''));
        error.output.payload.message = error.message;
    }

    error.reformat();
    return error;
};


internals.reformat = function () {

    this.output.payload.statusCode = this.output.statusCode;
    this.output.payload.error = internals.STATUS_CODES[this.output.statusCode] || 'Unknown';

    if (this.output.statusCode === 500) {
        this.output.payload.message = 'An internal server error occurred';              // Hide actual error from user
    }
    else if (this.message) {
        this.output.payload.message = this.message;
    }
};


// 4xx Client Errors

exports.badRequest = function (message, data) {

    return internals.create(400, message, data, exports.badRequest);
};


exports.unauthorized = function (message, scheme, attributes) {          // Or function (message, wwwAuthenticate[])

    const err = internals.create(401, message, undefined, exports.unauthorized);

    if (!scheme) {
        return err;
    }

    let wwwAuthenticate = '';

    if (typeof scheme === 'string') {

        // function (message, scheme, attributes)

        wwwAuthenticate = scheme;

        if (attributes || message) {
            err.output.payload.attributes = {};
        }

        if (attributes) {
            if (typeof attributes === 'string') {
                wwwAuthenticate = wwwAuthenticate + ' ' + Hoek.escapeHeaderAttribute(attributes);
                err.output.payload.attributes = attributes;
            }
            else {
                const names = Object.keys(attributes);
                for (let i = 0; i < names.length; ++i) {
                    const name = names[i];
                    if (i) {
                        wwwAuthenticate = wwwAuthenticate + ',';
                    }

                    let value = attributes[name];
                    if (value === null ||
                        value === undefined) {              // Value can be zero

                        value = '';
                    }
                    wwwAuthenticate = wwwAuthenticate + ' ' + name + '="' + Hoek.escapeHeaderAttribute(value.toString()) + '"';
                    err.output.payload.attributes[name] = value;
                }
            }
        }


        if (message) {
            if (attributes) {
                wwwAuthenticate = wwwAuthenticate + ',';
            }
            wwwAuthenticate = wwwAuthenticate + ' error="' + Hoek.escapeHeaderAttribute(message) + '"';
            err.output.payload.attributes.error = message;
        }
        else {
            err.isMissing = true;
        }
    }
    else {

        // function (message, wwwAuthenticate[])

        const wwwArray = scheme;
        for (let i = 0; i < wwwArray.length; ++i) {
            if (i) {
                wwwAuthenticate = wwwAuthenticate + ', ';
            }

            wwwAuthenticate = wwwAuthenticate + wwwArray[i];
        }
    }

    err.output.headers['WWW-Authenticate'] = wwwAuthenticate;

    return err;
};


exports.paymentRequired = function (message, data) {

    return internals.create(402, message, data, exports.paymentRequired);
};


exports.forbidden = function (message, data) {

    return internals.create(403, message, data, exports.forbidden);
};


exports.notFound = function (message, data) {

    return internals.create(404, message, data, exports.notFound);
};


exports.methodNotAllowed = function (message, data, allow) {

    const err = internals.create(405, message, data, exports.methodNotAllowed);

    if (typeof allow === 'string') {
        allow = [allow];
    }

    if (Array.isArray(allow)) {
        err.output.headers.Allow = allow.join(', ');
    }

    return err;
};


exports.notAcceptable = function (message, data) {

    return internals.create(406, message, data, exports.notAcceptable);
};


exports.proxyAuthRequired = function (message, data) {

    return internals.create(407, message, data, exports.proxyAuthRequired);
};


exports.clientTimeout = function (message, data) {

    return internals.create(408, message, data, exports.clientTimeout);
};


exports.conflict = function (message, data) {

    return internals.create(409, message, data, exports.conflict);
};


exports.resourceGone = function (message, data) {

    return internals.create(410, message, data, exports.resourceGone);
};


exports.lengthRequired = function (message, data) {

    return internals.create(411, message, data, exports.lengthRequired);
};


exports.preconditionFailed = function (message, data) {

    return internals.create(412, message, data, exports.preconditionFailed);
};


exports.entityTooLarge = function (message, data) {

    return internals.create(413, message, data, exports.entityTooLarge);
};


exports.uriTooLong = function (message, data) {

    return internals.create(414, message, data, exports.uriTooLong);
};


exports.unsupportedMediaType = function (message, data) {

    return internals.create(415, message, data, exports.unsupportedMediaType);
};


exports.rangeNotSatisfiable = function (message, data) {

    return internals.create(416, message, data, exports.rangeNotSatisfiable);
};


exports.expectationFailed = function (message, data) {

    return internals.create(417, message, data, exports.expectationFailed);
};


exports.teapot = function (message, data) {

    return internals.create(418, message, data, exports.teapot);
};


exports.badData = function (message, data) {

    return internals.create(422, message, data, exports.badData);
};


exports.locked = function (message, data) {

    return internals.create(423, message, data, exports.locked);
};


exports.preconditionRequired = function (message, data) {

    return internals.create(428, message, data, exports.preconditionRequired);
};


exports.tooManyRequests = function (message, data) {

    return internals.create(429, message, data, exports.tooManyRequests);
};


exports.illegal = function (message, data) {

    return internals.create(451, message, data, exports.illegal);
};


// 5xx Server Errors

exports.internal = function (message, data, statusCode) {

    return internals.serverError(message, data, statusCode, exports.internal);
};


internals.serverError = function (message, data, statusCode, ctor) {

    if (data instanceof Error &&
        !data.isBoom) {

        return exports.wrap(data, statusCode, message);
    }

    const error = internals.create(statusCode || 500, message, undefined, ctor);
    error.data = data;
    return error;
};


exports.notImplemented = function (message, data) {

    return internals.serverError(message, data, 501, exports.notImplemented);
};


exports.badGateway = function (message, data) {

    return internals.serverError(message, data, 502, exports.badGateway);
};


exports.serverUnavailable = function (message, data) {

    return internals.serverError(message, data, 503, exports.serverUnavailable);
};


exports.gatewayTimeout = function (message, data) {

    return internals.serverError(message, data, 504, exports.gatewayTimeout);
};


exports.badImplementation = function (message, data) {

    const err = internals.serverError(message, data, 500, exports.badImplementation);
    err.isDeveloperError = true;
    return err;
};
