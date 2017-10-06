'use strict';

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.validate = validate;
exports.afterRequest = afterRequest;
exports.beforeRequest = beforeRequest;
exports.browser = browser;
exports.cache = cache;
exports.content = content;
exports.cookie = cookie;
exports.creator = creator;
exports.entry = entry;
exports.har = har;
exports.header = header;
exports.log = log;
exports.page = page;
exports.pageTimings = pageTimings;
exports.postData = postData;
exports.query = query;
exports.request = request;
exports.response = response;
exports.timings = timings;

var _harSchema = require('har-schema');

var schemas = _interopRequireWildcard(_harSchema);

var _ajv = require('ajv');

var _ajv2 = _interopRequireDefault(_ajv);

var _error = require('./error');

var _error2 = _interopRequireDefault(_error);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

var ajv = void 0;

function validate(name) {
  var data = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
  var next = arguments[2];

  // validator config
  ajv = ajv || new _ajv2.default({
    allErrors: true,
    schemas: schemas
  });

  var validate = ajv.getSchema(name + '.json');

  var valid = validate(data);

  // callback?
  if (typeof next === 'function') {
    return next(!valid ? new _error2.default(validate.errors) : null, valid);
  }

  return valid;
}

function afterRequest(data, next) {
  return validate('afterRequest', data, next);
}

function beforeRequest(data, next) {
  return validate('beforeRequest', data, next);
}

function browser(data, next) {
  return validate('browser', data, next);
}

function cache(data, next) {
  return validate('cache', data, next);
}

function content(data, next) {
  return validate('content', data, next);
}

function cookie(data, next) {
  return validate('cookie', data, next);
}

function creator(data, next) {
  return validate('creator', data, next);
}

function entry(data, next) {
  return validate('entry', data, next);
}

function har(data, next) {
  return validate('har', data, next);
}

function header(data, next) {
  return validate('header', data, next);
}

function log(data, next) {
  return validate('log', data, next);
}

function page(data, next) {
  return validate('page', data, next);
}

function pageTimings(data, next) {
  return validate('pageTimings', data, next);
}

function postData(data, next) {
  return validate('postData', data, next);
}

function query(data, next) {
  return validate('query', data, next);
}

function request(data, next) {
  return validate('request', data, next);
}

function response(data, next) {
  return validate('response', data, next);
}

function timings(data, next) {
  return validate('timings', data, next);
}