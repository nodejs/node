/**
 * @typedef {import('./core.js').HChild} Child Acceptable child value
 * @typedef {import('./core.js').HProperties} Properties Acceptable properties value.
 *
 * @typedef {import('./jsx-classic').Element} s.JSX.Element
 * @typedef {import('./jsx-classic').IntrinsicAttributes} s.JSX.IntrinsicAttributes
 * @typedef {import('./jsx-classic').IntrinsicElements} s.JSX.IntrinsicElements
 * @typedef {import('./jsx-classic').ElementChildrenAttribute} s.JSX.ElementChildrenAttribute
 */

import {svg} from 'property-information'
import {core} from './core.js'
import {svgCaseSensitiveTagNames} from './svg-case-sensitive-tag-names.js'

export const s = core(svg, 'g', svgCaseSensitiveTagNames)
