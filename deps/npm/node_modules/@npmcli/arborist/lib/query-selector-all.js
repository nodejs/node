'use strict'

const { resolve } = require('path')
const { parser, arrayDelimiter } = require('@npmcli/query')
const localeCompare = require('@isaacs/string-locale-compare')('en')
const npa = require('npm-package-arg')
const minimatch = require('minimatch')
const semver = require('semver')

// handle results for parsed query asts, results are stored in a map that has a
// key that points to each ast selector node and stores the resulting array of
// arborist nodes as its value, that is essential to how we handle multiple
// query selectors, e.g: `#a, #b, #c` <- 3 diff ast selector nodes
class Results {
  #currentAstSelector
  #initialItems
  #inventory
  #pendingCombinator
  #results = new Map()
  #targetNode

  constructor (opts) {
    this.#currentAstSelector = opts.rootAstNode.nodes[0]
    this.#inventory = opts.inventory
    this.#initialItems = opts.initialItems
    this.#targetNode = opts.targetNode

    this.currentResults = this.#initialItems

    // reset by rootAstNode walker
    this.currentAstNode = opts.rootAstNode
  }

  get currentResults () {
    return this.#results.get(this.#currentAstSelector)
  }

  set currentResults (value) {
    this.#results.set(this.#currentAstSelector, value)
  }

  // retrieves the initial items to which start the filtering / matching
  // for most of the different types of recognized ast nodes, e.g: class (aka
  // depType), id, *, etc in different contexts we need to start with the
  // current list of filtered results, for example a query for `.workspace`
  // actually means the same as `*.workspace` so we want to start with the full
  // inventory if that's the first ast node we're reading but if it appears in
  // the middle of a query it should respect the previous filtered results,
  // combinators are a special case in which we always want to have the
  // complete inventory list in order to use the left-hand side ast node as a
  // filter combined with the element on its right-hand side
  get initialItems () {
    const firstParsed =
      (this.currentAstNode.parent.nodes[0] === this.currentAstNode) &&
      (this.currentAstNode.parent.parent.type === 'root')

    if (firstParsed) {
      return this.#initialItems
    }
    if (this.currentAstNode.prev().type === 'combinator') {
      return this.#inventory
    }
    return this.currentResults
  }

  // combinators need information about previously filtered items along
  // with info of the items parsed / retrieved from the selector right
  // past the combinator, for this reason combinators are stored and
  // only ran as the last part of each selector logic
  processPendingCombinator (nextResults) {
    if (this.#pendingCombinator) {
      const res = this.#pendingCombinator(this.currentResults, nextResults)
      this.#pendingCombinator = null
      this.currentResults = res
    } else {
      this.currentResults = nextResults
    }
  }

  // when collecting results to a root astNode, we traverse the list of child
  // selector nodes and collect all of their resulting arborist nodes into a
  // single/flat Set of items, this ensures we also deduplicate items
  collect (rootAstNode) {
    return new Set(rootAstNode.nodes.flatMap(n => this.#results.get(n)))
  }

  // selector types map to the '.type' property of the ast nodes via `${astNode.type}Type`
  //
  // attribute selector [name=value], etc
  attributeType () {
    const nextResults = this.initialItems.filter(node =>
      attributeMatch(this.currentAstNode, node.package)
    )
    this.processPendingCombinator(nextResults)
  }

  // dependency type selector (i.e. .prod, .dev, etc)
  // css calls this class, we interpret is as dependency type
  classType () {
    const depTypeFn = depTypes[String(this.currentAstNode)]
    if (!depTypeFn) {
      throw Object.assign(
        new Error(`\`${String(this.currentAstNode)}\` is not a supported dependency type.`),
        { code: 'EQUERYNODEPTYPE' }
      )
    }
    const nextResults = depTypeFn(this.initialItems)
    this.processPendingCombinator(nextResults)
  }

  // combinators (i.e. '>', ' ', '~')
  combinatorType () {
    this.#pendingCombinator = combinators[String(this.currentAstNode)]
  }

  // name selectors (i.e. #foo, #foo@1.0.0)
  // css calls this id, we interpret it as name
  idType () {
    const spec = npa(this.currentAstNode.value)
    const nextResults = this.initialItems.filter(node =>
      (node.name === spec.name || node.package.name === spec.name) &&
      (semver.satisfies(node.version, spec.fetchSpec) || !spec.rawSpec))
    this.processPendingCombinator(nextResults)
  }

  // pseudo selectors (prefixed with :)
  pseudoType () {
    const pseudoFn = `${this.currentAstNode.value.slice(1)}Pseudo`
    if (!this[pseudoFn]) {
      throw Object.assign(
        new Error(`\`${this.currentAstNode.value
        }\` is not a supported pseudo selector.`),
        { code: 'EQUERYNOPSEUDO' }
      )
    }
    const nextResults = this[pseudoFn]()
    this.processPendingCombinator(nextResults)
  }

  selectorType () {
    this.#currentAstSelector = this.currentAstNode
    // starts a new array in which resulting items
    // can be stored for each given ast selector
    if (!this.currentResults) {
      this.currentResults = []
    }
  }

  universalType () {
    this.processPendingCombinator(this.initialItems)
  }

  // pseudo selectors map to the 'value' property of the pseudo selectors in the ast nodes
  // via selectors via `${value.slice(1)}Pseudo`
  attrPseudo () {
    const { lookupProperties, attributeMatcher } = this.currentAstNode

    return this.initialItems.filter(node => {
      let objs = [node.package]
      for (const prop of lookupProperties) {
        // if an isArray symbol is found that means we'll need to iterate
        // over the previous found array to basically make sure we traverse
        // all its indexes testing for possible objects that may eventually
        // hold more keys specified in a selector
        if (prop === arrayDelimiter) {
          objs = objs.flat()
          continue
        }

        // otherwise just maps all currently found objs
        // to the next prop from the lookup properties list,
        // filters out any empty key lookup
        objs = objs.flatMap(obj => obj[prop] || [])

        // in case there's no property found in the lookup
        // just filters that item out
        const noAttr = objs.every(obj => !obj)
        if (noAttr) {
          return false
        }
      }

      // if any of the potential object matches
      // that item should be in the final result
      return objs.some(obj => attributeMatch(attributeMatcher, obj))
    })
  }

  emptyPseudo () {
    return this.initialItems.filter(node => node.edgesOut.size === 0)
  }

  extraneousPseudo () {
    return this.initialItems.filter(node => node.extraneous)
  }

  hasPseudo () {
    const found = []
    for (const item of this.initialItems) {
      const res = retrieveNodesFromParsedAst({
        // This is the one time initialItems differs from inventory
        initialItems: [item],
        inventory: this.#inventory,
        rootAstNode: this.currentAstNode.nestedNode,
        targetNode: item,
      })
      if (res.size > 0) {
        found.push(item)
      }
    }
    return found
  }

  invalidPseudo () {
    const found = []
    for (const node of this.initialItems) {
      for (const edge of node.edgesIn) {
        if (edge.invalid) {
          found.push(node)
          break
        }
      }
    }
    return found
  }

  isPseudo () {
    const res = retrieveNodesFromParsedAst({
      initialItems: this.initialItems,
      inventory: this.#inventory,
      rootAstNode: this.currentAstNode.nestedNode,
      targetNode: this.currentAstNode,
    })
    return [...res]
  }

  linkPseudo () {
    return this.initialItems.filter(node => node.isLink || (node.isTop && !node.isRoot))
  }

  missingPseudo () {
    return this.#inventory.reduce((res, node) => {
      for (const edge of node.edgesOut.values()) {
        if (edge.missing) {
          const pkg = { name: edge.name, version: edge.spec }
          res.push(new this.#targetNode.constructor({ pkg }))
        }
      }
      return res
    }, [])
  }

  notPseudo () {
    const res = retrieveNodesFromParsedAst({
      initialItems: this.initialItems,
      inventory: this.#inventory,
      rootAstNode: this.currentAstNode.nestedNode,
      targetNode: this.currentAstNode,
    })
    const internalSelector = new Set(res)
    return this.initialItems.filter(node =>
      !internalSelector.has(node))
  }

  pathPseudo () {
    return this.initialItems.filter(node => {
      if (!this.currentAstNode.pathValue) {
        return true
      }
      return minimatch(
        node.realpath.replace(/\\+/g, '/'),
        resolve(node.root.realpath, this.currentAstNode.pathValue).replace(/\\+/g, '/')
      )
    })
  }

  privatePseudo () {
    return this.initialItems.filter(node => node.package.private)
  }

  rootPseudo () {
    return this.initialItems.filter(node => node === this.#targetNode.root)
  }

  scopePseudo () {
    return this.initialItems.filter(node => node === this.#targetNode)
  }

  semverPseudo () {
    if (!this.currentAstNode.semverValue) {
      return this.initialItems
    }
    return this.initialItems.filter(node =>
      semver.satisfies(node.version, this.currentAstNode.semverValue))
  }

  typePseudo () {
    if (!this.currentAstNode.typeValue) {
      return this.initialItems
    }
    return this.initialItems
      .flatMap(node => {
        const found = []
        for (const edge of node.edgesIn) {
          if (npa(`${edge.name}@${edge.spec}`).type === this.currentAstNode.typeValue) {
            found.push(edge.to)
          }
        }
        return found
      })
  }

  dedupedPseudo () {
    return this.initialItems.filter(node => node.target.edgesIn.size > 1)
  }
}

// operators for attribute selectors
const attributeOperators = {
  // attribute value is equivalent
  '=' ({ attr, value, insensitive }) {
    return attr === value
  },
  // attribute value contains word
  '~=' ({ attr, value, insensitive }) {
    return (attr.match(/\w+/g) || []).includes(value)
  },
  // attribute value contains string
  '*=' ({ attr, value, insensitive }) {
    return attr.includes(value)
  },
  // attribute value is equal or starts with
  '|=' ({ attr, value, insensitive }) {
    return attr.startsWith(`${value}-`)
  },
  // attribute value starts with
  '^=' ({ attr, value, insensitive }) {
    return attr.startsWith(value)
  },
  // attribute value ends with
  '$=' ({ attr, value, insensitive }) {
    return attr.endsWith(value)
  },
}

const attributeOperator = ({ attr, value, insensitive, operator }) => {
  if (typeof attr === 'number') {
    attr = String(attr)
  }
  if (typeof attr !== 'string') {
    // It's an object or an array, bail
    return false
  }
  if (insensitive) {
    attr = attr.toLowerCase()
  }
  return attributeOperators[operator]({
    attr,
    insensitive,
    value,
  })
}

const attributeMatch = (matcher, obj) => {
  const insensitive = !!matcher.insensitive
  const operator = matcher.operator || ''
  const attribute = matcher.qualifiedAttribute
  let value = matcher.value || ''
  // return early if checking existence
  if (operator === '') {
    return Boolean(obj[attribute])
  }
  if (insensitive) {
    value = value.toLowerCase()
  }
  // in case the current object is an array
  // then we try to match every item in the array
  if (Array.isArray(obj[attribute])) {
    return obj[attribute].find((i, index) => {
      const attr = obj[attribute][index] || ''
      return attributeOperator({ attr, value, insensitive, operator })
    })
  } else {
    const attr = obj[attribute] || ''
    return attributeOperator({ attr, value, insensitive, operator })
  }
}

const edgeIsType = (node, type, seen = new Set()) => {
  for (const edgeIn of node.edgesIn) {
    // TODO Need a test with an infinite loop
    if (seen.has(edgeIn)) {
      continue
    }
    seen.add(edgeIn)
    if (edgeIn.type === type || edgeIn.from[type] || edgeIsType(edgeIn.from, type, seen)) {
      return true
    }
  }
  return false
}

const filterByType = (nodes, type) => {
  const found = []
  for (const node of nodes) {
    if (node[type] || edgeIsType(node, type)) {
      found.push(node)
    }
  }
  return found
}

const depTypes = {
  // dependency
  '.prod' (prevResults) {
    const found = []
    for (const node of prevResults) {
      if (!node.dev) {
        found.push(node)
      }
    }
    return found
  },
  // devDependency
  '.dev' (prevResults) {
    return filterByType(prevResults, 'dev')
  },
  // optionalDependency
  '.optional' (prevResults) {
    return filterByType(prevResults, 'optional')
  },
  // peerDependency
  '.peer' (prevResults) {
    return filterByType(prevResults, 'peer')
  },
  // workspace
  '.workspace' (prevResults) {
    return prevResults.filter(node => node.isWorkspace)
  },
  // bundledDependency
  '.bundled' (prevResults) {
    return prevResults.filter(node => node.inBundle)
  },
}

// checks if a given node has a direct parent in any of the nodes provided in
// the compare nodes array
const hasParent = (node, compareNodes) => {
  // All it takes is one so we loop and return on the first hit
  for (const compareNode of compareNodes) {
    // follows logical parent for link anscestors
    if (node.isTop && (node.resolveParent === compareNode)) {
      return true
    }
    // follows edges-in to check if they match a possible parent
    for (const edge of node.edgesIn) {
      if (edge && edge.from === compareNode) {
        return true
      }
    }
  }
  return false
}

// checks if a given node is a descendant of any of the nodes provided in the
// compareNodes array
const hasAscendant = (node, compareNodes, seen = new Set()) => {
  // TODO (future) loop over ancestry property
  if (hasParent(node, compareNodes)) {
    return true
  }

  if (node.isTop && node.resolveParent) {
    return hasAscendant(node.resolveParent, compareNodes)
  }
  for (const edge of node.edgesIn) {
    // TODO Need a test with an infinite loop
    if (seen.has(edge)) {
      continue
    }
    seen.add(edge)
    if (edge && edge.from && hasAscendant(edge.from, compareNodes, seen)) {
      return true
    }
  }
  return false
}

const combinators = {
  // direct descendant
  '>' (prevResults, nextResults) {
    return nextResults.filter(node => hasParent(node, prevResults))
  },
  // any descendant
  ' ' (prevResults, nextResults) {
    return nextResults.filter(node => hasAscendant(node, prevResults))
  },
  // sibling
  '~' (prevResults, nextResults) {
    // Return any node in nextResults that is a sibling of (aka shares a
    // parent with) a node in prevResults
    const parentNodes = new Set() // Parents of everything in prevResults
    for (const node of prevResults) {
      for (const edge of node.edgesIn) {
        // edge.from always exists cause it's from another node's edgesIn
        parentNodes.add(edge.from)
      }
    }
    return nextResults.filter(node =>
      !prevResults.includes(node) && hasParent(node, [...parentNodes])
    )
  },
}

const retrieveNodesFromParsedAst = (opts) => {
  // when we first call this it's the parsed query.  all other times it's
  // results.currentNode.nestedNode
  const rootAstNode = opts.rootAstNode

  if (!rootAstNode.nodes) {
    return new Set()
  }

  const results = new Results(opts)

  rootAstNode.walk((nextAstNode) => {
    // This is the only place we reset currentAstNode
    results.currentAstNode = nextAstNode
    const updateFn = `${results.currentAstNode.type}Type`
    if (typeof results[updateFn] !== 'function') {
      throw Object.assign(
        new Error(`\`${results.currentAstNode.type}\` is not a supported selector.`),
        { code: 'EQUERYNOSELECTOR' }
      )
    }
    results[updateFn]()
  })

  return results.collect(rootAstNode)
}

// We are keeping this async in the event that we do add async operators, we
// won't have to have a breaking change on this function signature.
const querySelectorAll = async (targetNode, query) => {
  // This never changes ever we just pass it around. But we can't scope it to
  // this whole file if we ever want to support concurrent calls to this
  // function.
  const inventory = [...targetNode.root.inventory.values()]
  // res is a Set of items returned for each parsed css ast selector
  const res = retrieveNodesFromParsedAst({
    initialItems: inventory,
    inventory,
    rootAstNode: parser(query),
    targetNode,
  })

  // returns nodes ordered by realpath
  return [...res].sort((a, b) => localeCompare(a.location, b.location))
}

module.exports = querySelectorAll
