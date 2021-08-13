/**
 * @typedef {import('mdast').Link} Link
 * @typedef {import('mdast-util-from-markdown').Extension} FromMarkdownExtension
 * @typedef {import('mdast-util-from-markdown').Transform} FromMarkdownTransform
 * @typedef {import('mdast-util-from-markdown').Handle} FromMarkdownHandle
 * @typedef {import('mdast-util-to-markdown/lib/types.js').Options} ToMarkdownExtension
 * @typedef {import('mdast-util-find-and-replace').ReplaceFunction} ReplaceFunction
 * @typedef {import('mdast-util-find-and-replace').RegExpMatchObject} RegExpMatchObject
 * @typedef {import('mdast-util-find-and-replace').PhrasingContent} PhrasingContent
 */

import {ccount} from 'ccount'
import {findAndReplace} from 'mdast-util-find-and-replace'
import {unicodePunctuation, unicodeWhitespace} from 'micromark-util-character'

const inConstruct = 'phrasing'
const notInConstruct = ['autolink', 'link', 'image', 'label']

/** @type {FromMarkdownExtension} */
export const gfmAutolinkLiteralFromMarkdown = {
  transforms: [transformGfmAutolinkLiterals],
  enter: {
    literalAutolink: enterLiteralAutolink,
    literalAutolinkEmail: enterLiteralAutolinkValue,
    literalAutolinkHttp: enterLiteralAutolinkValue,
    literalAutolinkWww: enterLiteralAutolinkValue
  },
  exit: {
    literalAutolink: exitLiteralAutolink,
    literalAutolinkEmail: exitLiteralAutolinkEmail,
    literalAutolinkHttp: exitLiteralAutolinkHttp,
    literalAutolinkWww: exitLiteralAutolinkWww
  }
}

/** @type {ToMarkdownExtension} */
export const gfmAutolinkLiteralToMarkdown = {
  unsafe: [
    {
      character: '@',
      before: '[+\\-.\\w]',
      after: '[\\-.\\w]',
      inConstruct,
      notInConstruct
    },
    {
      character: '.',
      before: '[Ww]',
      after: '[\\-.\\w]',
      inConstruct,
      notInConstruct
    },
    {character: ':', before: '[ps]', after: '\\/', inConstruct, notInConstruct}
  ]
}

/** @type {FromMarkdownHandle} */
function enterLiteralAutolink(token) {
  this.enter({type: 'link', title: null, url: '', children: []}, token)
}

/** @type {FromMarkdownHandle} */
function enterLiteralAutolinkValue(token) {
  this.config.enter.autolinkProtocol.call(this, token)
}

/** @type {FromMarkdownHandle} */
function exitLiteralAutolinkHttp(token) {
  this.config.exit.autolinkProtocol.call(this, token)
}

/** @type {FromMarkdownHandle} */
function exitLiteralAutolinkWww(token) {
  this.config.exit.data.call(this, token)
  const node = /** @type {Link} */ (this.stack[this.stack.length - 1])
  node.url = 'http://' + this.sliceSerialize(token)
}

/** @type {FromMarkdownHandle} */
function exitLiteralAutolinkEmail(token) {
  this.config.exit.autolinkEmail.call(this, token)
}

/** @type {FromMarkdownHandle} */
function exitLiteralAutolink(token) {
  this.exit(token)
}

/** @type {FromMarkdownTransform} */
function transformGfmAutolinkLiterals(tree) {
  findAndReplace(
    tree,
    [
      [/(https?:\/\/|www(?=\.))([-.\w]+)([^ \t\r\n]*)/gi, findUrl],
      [/([-.\w+]+)@([-\w]+(?:\.[-\w]+)+)/g, findEmail]
    ],
    {ignore: ['link', 'linkReference']}
  )
}

/**
 * @type {ReplaceFunction}
 * @param {string} _
 * @param {string} protocol
 * @param {string} domain
 * @param {string} path
 * @param {RegExpMatchObject} match
 */
// eslint-disable-next-line max-params
function findUrl(_, protocol, domain, path, match) {
  let prefix = ''

  // Not an expected previous character.
  if (!previous(match)) {
    return false
  }

  // Treat `www` as part of the domain.
  if (/^w/i.test(protocol)) {
    domain = protocol + domain
    protocol = ''
    prefix = 'http://'
  }

  if (!isCorrectDomain(domain)) {
    return false
  }

  const parts = splitUrl(domain + path)

  if (!parts[0]) return false

  /** @type {PhrasingContent} */
  const result = {
    type: 'link',
    title: null,
    url: prefix + protocol + parts[0],
    children: [{type: 'text', value: protocol + parts[0]}]
  }

  if (parts[1]) {
    return [result, {type: 'text', value: parts[1]}]
  }

  return result
}

/**
 * @type {ReplaceFunction}
 * @param {string} _
 * @param {string} atext
 * @param {string} label
 * @param {RegExpMatchObject} match
 */
function findEmail(_, atext, label, match) {
  // Not an expected previous character.
  if (!previous(match, true) || /[_-]$/.test(label)) {
    return false
  }

  return {
    type: 'link',
    title: null,
    url: 'mailto:' + atext + '@' + label,
    children: [{type: 'text', value: atext + '@' + label}]
  }
}

/**
 * @param {string} domain
 * @returns {boolean}
 */
function isCorrectDomain(domain) {
  const parts = domain.split('.')

  if (
    parts.length < 2 ||
    (parts[parts.length - 1] &&
      (/_/.test(parts[parts.length - 1]) ||
        !/[a-zA-Z\d]/.test(parts[parts.length - 1]))) ||
    (parts[parts.length - 2] &&
      (/_/.test(parts[parts.length - 2]) ||
        !/[a-zA-Z\d]/.test(parts[parts.length - 2])))
  ) {
    return false
  }

  return true
}

/**
 * @param {string} url
 * @returns {[string, string|undefined]}
 */
function splitUrl(url) {
  const trailExec = /[!"&'),.:;<>?\]}]+$/.exec(url)
  /** @type {number} */
  let closingParenIndex
  /** @type {number} */
  let openingParens
  /** @type {number} */
  let closingParens
  /** @type {string|undefined} */
  let trail

  if (trailExec) {
    url = url.slice(0, trailExec.index)
    trail = trailExec[0]
    closingParenIndex = trail.indexOf(')')
    openingParens = ccount(url, '(')
    closingParens = ccount(url, ')')

    while (closingParenIndex !== -1 && openingParens > closingParens) {
      url += trail.slice(0, closingParenIndex + 1)
      trail = trail.slice(closingParenIndex + 1)
      closingParenIndex = trail.indexOf(')')
      closingParens++
    }
  }

  return [url, trail]
}

/**
 * @param {RegExpMatchObject} match
 * @param {boolean} [email=false]
 * @returns {boolean}
 */
function previous(match, email) {
  const code = match.input.charCodeAt(match.index - 1)

  return (
    (match.index === 0 ||
      unicodeWhitespace(code) ||
      unicodePunctuation(code)) &&
    (!email || code !== 47)
  )
}
