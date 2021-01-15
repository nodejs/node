// a tree representing the difference between two trees
// A Diff node's parent is not necessarily the parent of
// the node location it refers to, but rather the highest level
// node that needs to be either changed or removed.
// Thus, the root Diff node is the shallowest change required
// for a given branch of the tree being mutated.

const {depth} = require('treeverse')
const {existsSync} = require('fs')

const ssri = require('ssri')

class Diff {
  constructor ({actual, ideal}) {
    this.children = []
    this.actual = actual
    this.ideal = ideal
    if (this.ideal) {
      this.resolved = this.ideal.resolved
      this.integrity = this.ideal.integrity
    }
    this.action = getAction(this)
    this.parent = null
    // the set of leaf nodes that we rake up to the top level
    this.leaves = []
    // the set of nodes that don't change in this branch of the tree
    this.unchanged = []
    // the set of nodes that will be removed in this branch of the tree
    this.removed = []
  }

  static calculate ({actual, ideal}) {
    return depth({
      tree: new Diff({actual, ideal}),
      getChildren,
      leave,
    })
  }
}

const getAction = ({actual, ideal}) => {
  if (!ideal)
    return 'REMOVE'

  // bundled meta-deps are copied over to the ideal tree when we visit it,
  // so they'll appear to be missing here.  There's no need to handle them
  // in the diff, though, because they'll be replaced at reify time anyway
  // Otherwise, add the missing node.
  if (!actual)
    return ideal.inDepBundle ? null : 'ADD'

  // always ignore the root node
  if (ideal.isRoot && actual.isRoot)
    return null

  const binsExist = ideal.binPaths.every((path) => existsSync(path))

  // top nodes, links, and git deps won't have integrity, but do have resolved
  if (!ideal.integrity && !actual.integrity && ideal.resolved === actual.resolved && binsExist)
    return null

  // otherwise, verify that it's the same bits
  // note that if ideal has integrity, and resolved doesn't, we treat
  // that as a 'change', so that it gets re-fetched and locked down.
  if (!ideal.integrity || !actual.integrity || !ssri.parse(ideal.integrity).match(actual.integrity) || !binsExist)
    return 'CHANGE'

  return null
}

const allChildren = node => {
  if (!node)
    return new Map()

  // if the node is a global root, and also a link, then what we really
  // want is to traverse the target's children
  if (node.global && node.isRoot && node.isLink)
    return allChildren(node.target)

  const kids = new Map()
  for (const n of [node, ...node.fsChildren]) {
    for (const kid of n.children.values())
      kids.set(kid.path, kid)
  }
  return kids
}

// functions for the walk options when we traverse the trees
// to create the diff tree
const getChildren = diff => {
  const children = []
  const {unchanged, removed} = diff

  // Note: we DON'T diff fsChildren themselves, because they are either
  // included in the package contents, or part of some other project, and
  // will never appear in legacy shrinkwraps anyway.  but we _do_ include the
  // child nodes of fsChildren, because those are nodes that we are typically
  // responsible for installing.
  const actualKids = allChildren(diff.actual)
  const idealKids = allChildren(diff.ideal)
  const paths = new Set([...actualKids.keys(), ...idealKids.keys()])
  for (const path of paths) {
    const actual = actualKids.get(path)
    const ideal = idealKids.get(path)
    diffNode(actual, ideal, children, unchanged, removed)
  }

  if (diff.leaves && !children.length)
    diff.leaves.push(diff)

  return children
}

const diffNode = (actual, ideal, children, unchanged, removed) => {
  const action = getAction({actual, ideal})

  // if it's a match, then get its children
  // otherwise, this is the child diff node
  if (action) {
    if (action === 'REMOVE')
      removed.push(actual)
    children.push(new Diff({actual, ideal}))
  } else {
    unchanged.push(ideal)
    // !*! Weird dirty hack warning !*!
    //
    // Bundled deps aren't loaded in the ideal tree, because we don't know
    // what they are going to be without unpacking.  Swap them over now if
    // the bundling node isn't changing, so we don't prune them later.
    //
    // It's a little bit dirty to be doing this here, since it means that
    // diffing trees can mutate them, but otherwise we have to walk over
    // all unchanging bundlers and correct the diff later, so it's more
    // efficient to just fix it while we're passing through already.
    //
    // Note that moving over a bundled dep will break the links to other
    // deps under this parent, which may have been transitively bundled.
    // Breaking those links means that we'll no longer see the transitive
    // dependency, meaning that it won't appear as bundled any longer!
    // In order to not end up dropping transitively bundled deps, we have
    // to get the list of nodes to move, then move them all at once, rather
    // than moving them one at a time in the first loop.
    const bd = ideal.package.bundleDependencies
    if (actual && bd && bd.length) {
      const bundledChildren = []
      for (const node of actual.children.values()) {
        if (node.inBundle)
          bundledChildren.push(node)
      }
      for (const node of bundledChildren)
        node.parent = ideal
    }
    children.push(...getChildren({actual, ideal, unchanged, removed}))
  }
}

// set the parentage in the leave step so that we aren't attaching
// child nodes only to remove them later.  also bubble up the unchanged
// nodes so that we can move them out of staging in the reification step.
const leave = (diff, children) => {
  children.forEach(kid => {
    kid.parent = diff
    diff.leaves.push(...kid.leaves)
    diff.unchanged.push(...kid.unchanged)
    diff.removed.push(...kid.removed)
  })
  diff.children = children
  return diff
}

module.exports = Diff
