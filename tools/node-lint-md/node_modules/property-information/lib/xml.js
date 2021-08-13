import {create} from './util/create.js'

export var xml = create({
  space: 'xml',
  transform: xmlTransform,
  properties: {xmlLang: null, xmlBase: null, xmlSpace: null}
})

/**
 * @param {unknown} _
 * @param {string} prop
 * @returns {string}
 */
function xmlTransform(_, prop) {
  return 'xml:' + prop.slice(3).toLowerCase()
}
