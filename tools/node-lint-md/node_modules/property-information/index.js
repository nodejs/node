/**
 * @typedef {import('./lib/util/info.js').Info} Info
 * @typedef {import('./lib/util/schema.js').Schema} Schema
 */

import {merge} from './lib/util/merge.js'
import {xlink} from './lib/xlink.js'
import {xml} from './lib/xml.js'
import {xmlns} from './lib/xmlns.js'
import {aria} from './lib/aria.js'
import {html as htmlBase} from './lib/html.js'
import {svg as svgBase} from './lib/svg.js'

export {find} from './lib/find.js'
export {hastToReact} from './lib/hast-to-react.js'
export {normalize} from './lib/normalize.js'
export var html = merge([xml, xlink, xmlns, aria, htmlBase], 'html')
export var svg = merge([xml, xlink, xmlns, aria, svgBase], 'svg')
