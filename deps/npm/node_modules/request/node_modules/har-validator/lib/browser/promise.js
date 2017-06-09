import * as schemas from 'har-schema';
import Ajv from 'ajv';
import HARError from './error';

let ajv;

export function validate(name, data = {}) {
  // validator config
  ajv = ajv || new Ajv({
    allErrors: true,
    schemas: schemas
  });

  let validate = ajv.getSchema(name + '.json');

  return new Promise((resolve, reject) => {
    let valid = validate(data);

    !valid ? reject(new HARError(validate.errors)) : resolve(data);
  });
}

export function afterRequest(data) {
  return validate('afterRequest', data);
}

export function beforeRequest(data) {
  return validate('beforeRequest', data);
}

export function browser(data) {
  return validate('browser', data);
}

export function cache(data) {
  return validate('cache', data);
}

export function content(data) {
  return validate('content', data);
}

export function cookie(data) {
  return validate('cookie', data);
}

export function creator(data) {
  return validate('creator', data);
}

export function entry(data) {
  return validate('entry', data);
}

export function har(data) {
  return validate('har', data);
}

export function header(data) {
  return validate('header', data);
}

export function log(data) {
  return validate('log', data);
}

export function page(data) {
  return validate('page', data);
}

export function pageTimings(data) {
  return validate('pageTimings', data);
}

export function postData(data) {
  return validate('postData', data);
}

export function query(data) {
  return validate('query', data);
}

export function request(data) {
  return validate('request', data);
}

export function response(data) {
  return validate('response', data);
}

export function timings(data) {
  return validate('timings', data);
}