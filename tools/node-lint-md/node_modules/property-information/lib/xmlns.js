import {create} from './util/create.js'
import {caseInsensitiveTransform} from './util/case-insensitive-transform.js'

export var xmlns = create({
  space: 'xmlns',
  attributes: {xmlnsxlink: 'xmlns:xlink'},
  transform: caseInsensitiveTransform,
  properties: {xmlns: null, xmlnsXLink: null}
})
