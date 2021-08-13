/**
 * @typedef {import('./core.js').HChild} Child Acceptable child value
 * @typedef {import('./core.js').HProperties} Properties Acceptable properties value.
 *
 * @typedef {import('./jsx-classic').Element} h.JSX.Element
 * @typedef {import('./jsx-classic').IntrinsicAttributes} h.JSX.IntrinsicAttributes
 * @typedef {import('./jsx-classic').IntrinsicElements} h.JSX.IntrinsicElements
 * @typedef {import('./jsx-classic').ElementChildrenAttribute} h.JSX.ElementChildrenAttribute
 */

import {html} from 'property-information'
import {core} from './core.js'

export const h = core(html, 'div')
