import * as schemas from 'har-schema'
import Ajv from 'ajv'
import HARError from './error'

let ajv

export function validate (name, data = {}, next) {
  // validator config
  ajv = ajv || new Ajv({
    allErrors: true,
    schemas: schemas
  })

  let validate = ajv.getSchema(name + '.json')

  let valid = validate(data)

  // callback?
  if (typeof next === 'function') {
    return next(!valid ? new HARError(validate.errors) : null, valid)
  }

  return valid
}

export function afterRequest (data, next) {
  return validate('afterRequest', data, next)
}

export function beforeRequest (data, next) {
  return validate('beforeRequest', data, next)
}

export function browser (data, next) {
  return validate('browser', data, next)
}

export function cache (data, next) {
  return validate('cache', data, next)
}

export function content (data, next) {
  return validate('content', data, next)
}

export function cookie (data, next) {
  return validate('cookie', data, next)
}

export function creator (data, next) {
  return validate('creator', data, next)
}

export function entry (data, next) {
  return validate('entry', data, next)
}

export function har (data, next) {
  return validate('har', data, next)
}

export function header (data, next) {
  return validate('header', data, next)
}

export function log (data, next) {
  return validate('log', data, next)
}

export function page (data, next) {
  return validate('page', data, next)
}

export function pageTimings (data, next) {
  return validate('pageTimings', data, next)
}

export function postData (data, next) {
  return validate('postData', data, next)
}

export function query (data, next) {
  return validate('query', data, next)
}

export function request (data, next) {
  return validate('request', data, next)
}

export function response (data, next) {
  return validate('response', data, next)
}

export function timings (data, next) {
  return validate('timings', data, next)
}
