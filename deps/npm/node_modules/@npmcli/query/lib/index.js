'use strict'

const parser = require('postcss-selector-parser')

const arrayDelimiter = Symbol('arrayDelimiter')

const escapeSlashes = str =>
  str.replace(/\//g, '\\/')

const unescapeSlashes = str =>
  str.replace(/\\\//g, '/')

// recursively fixes up any :attr pseudo-class found
const fixupAttr = astNode => {
  const properties = []
  const matcher = {}
  for (const selectorAstNode of astNode.nodes) {
    const [firstAstNode] = selectorAstNode.nodes
    if (firstAstNode.type === 'tag') {
      properties.push(firstAstNode.value)
    }
  }

  const lastSelectorAstNode = astNode.nodes.pop()
  const [attributeAstNode] = lastSelectorAstNode.nodes

  if (attributeAstNode.value === ':attr') {
    const appendParts = fixupAttr(attributeAstNode)
    properties.push(arrayDelimiter, ...appendParts.lookupProperties)
    matcher.qualifiedAttribute = appendParts.attributeMatcher.qualifiedAttribute
    matcher.operator = appendParts.attributeMatcher.operator
    matcher.value = appendParts.attributeMatcher.value

    // backwards compatibility
    matcher.attribute = appendParts.attributeMatcher.attribute

    if (appendParts.attributeMatcher.insensitive) {
      matcher.insensitive = true
    }
  } else {
    if (attributeAstNode.type !== 'attribute') {
      throw Object.assign(
        new Error('`:attr` pseudo-class expects an attribute matcher as the last value'),
        { code: 'EQUERYATTR' }
      )
    }

    matcher.qualifiedAttribute = unescapeSlashes(attributeAstNode.qualifiedAttribute)
    matcher.operator = attributeAstNode.operator
    matcher.value = attributeAstNode.value

    // backwards compatibility
    matcher.attribute = matcher.qualifiedAttribute

    if (attributeAstNode.insensitive) {
      matcher.insensitive = true
    }
  }

  astNode.lookupProperties = properties
  astNode.attributeMatcher = matcher
  astNode.nodes.length = 0
  return astNode
}

// fixed up nested pseudo nodes will have their internal selectors moved
// to a new root node that will be referenced by the `nestedNode` property,
// this tweak makes it simpler to reuse `retrieveNodesFromParsedAst` to
// recursively parse and extract results from the internal selectors
const fixupNestedPseudo = astNode => {
  // create a new ast root node and relocate any children
  // selectors of the current ast node to this new root
  const newRootNode = parser.root()
  astNode.nestedNode = newRootNode
  newRootNode.nodes = [...astNode.nodes]

  // clean up the ast by removing the children nodes from the
  // current ast node while also cleaning up their `parent` refs
  astNode.nodes.length = 0
  for (const currAstNode of newRootNode.nodes) {
    currAstNode.parent = newRootNode
  }

  // recursively fixup nodes of any nested selector
  transformAst(newRootNode)
}

// :semver(<version|range>, [selector], [function])
const fixupSemverSpecs = astNode => {
  // the first child node contains the version or range, most likely as a tag and a series of
  // classes. we combine them into a single string here. this is the only required input.
  const children = astNode.nodes.shift().nodes
  const value = children.reduce((res, i) => `${res}${String(i)}`, '')

  // next, if we have 2 nodes left then the user called us with a total of 3. that means the
  // last one tells us what specific semver function the user is requesting, so we pull that out
  let semverFunc
  if (astNode.nodes.length === 2) {
    const funcNode = astNode.nodes.pop().nodes[0]
    if (funcNode.type === 'tag') {
      semverFunc = funcNode.value
    }
  }

  // now if there's a node left, that node is our selector. since that is the last remaining
  // child node, we call fixupAttr on ourselves so that the attribute selectors get parsed
  if (astNode.nodes.length === 1) {
    fixupAttr(astNode)
  } else {
    // we weren't provided a selector, so we default to `[version]`. note, there's no default
    // operator here. that's because we don't know yet if the user has provided us a version
    // or range to assert against
    astNode.attributeMatcher = {
      insensitive: false,
      attribute: 'version',
      qualifiedAttribute: 'version',
    }
    astNode.lookupProperties = []
  }

  astNode.semverFunc = semverFunc
  astNode.semverValue = value
  astNode.nodes.length = 0
}

const fixupTypes = astNode => {
  const [valueAstNode] = astNode.nodes[0].nodes
  const { value } = valueAstNode || {}
  astNode.typeValue = value
  astNode.nodes.length = 0
}

const fixupPaths = astNode => {
  astNode.pathValue = unescapeSlashes(String(astNode.nodes[0]))
  astNode.nodes.length = 0
}

const fixupOutdated = astNode => {
  if (astNode.nodes.length) {
    astNode.outdatedKind = String(astNode.nodes[0])
    astNode.nodes.length = 0
  }
}

// a few of the supported ast nodes need to be tweaked in order to properly be
// interpreted as proper arborist query selectors, namely semver ranges from
// both ids and :semver pseudo-class selectors need to be translated from what
// are usually multiple ast nodes, such as: tag:1, class:.0, class:.0 to a
// single `1.0.0` value, other pseudo-class selectors also get preprocessed in
// order to make it simpler to execute later when traversing each ast node
// using rootNode.walk(), such as :path, :type, etc. transformAst handles all
// these modifications to the parsed ast by doing an extra, initial traversal
// of the parsed ast from the query and modifying the parsed nodes accordingly
const transformAst = selector => {
  selector.walk((nextAstNode) => {
    switch (nextAstNode.value) {
      case ':attr':
        return fixupAttr(nextAstNode)
      case ':is':
      case ':has':
      case ':not':
        return fixupNestedPseudo(nextAstNode)
      case ':path':
        return fixupPaths(nextAstNode)
      case ':semver':
        return fixupSemverSpecs(nextAstNode)
      case ':type':
        return fixupTypes(nextAstNode)
      case ':outdated':
        return fixupOutdated(nextAstNode)
    }
  })
}

const queryParser = (query) => {
  // if query is an empty string or any falsy
  // value, just returns an empty result
  if (!query) {
    return []
  }

  return parser(transformAst)
    .astSync(escapeSlashes(query), { lossless: false })
}

module.exports = {
  parser: queryParser,
  arrayDelimiter,
}
