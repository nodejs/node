// Internal methods used by buildIdealTree.
// Answer the question: "can I put this dep here?"
//
// IMPORTANT: *nothing* in this class should *ever* modify or mutate the tree
// at all.  The contract here is strictly limited to read operations.  We call
// this in the process of walking through the ideal tree checking many
// different potential placement targets for a given node.  If a change is made
// to the tree along the way, that can cause serious problems!
//
// In order to enforce this restriction, in debug mode, canPlaceDep() will
// snapshot the tree at the start of the process, and then at the end, will
// verify that it still matches the snapshot, and throw an error if any changes
// occurred.
//
// The algorithm is roughly like this:
// - check the node itself:
//   - if there is no version present, and no conflicting edges from target,
//     OK, provided all peers can be placed at or above the target.
//   - if the current version matches, KEEP
//   - if there is an older version present, which can be replaced, then
//     - if satisfying and preferDedupe? KEEP
//     - else: REPLACE
//   - if there is a newer version present, and preferDedupe, REPLACE
//   - if the version present satisfies the edge, KEEP
//   - else: CONFLICT
// - if the node is not in conflict, check each of its peers:
//   - if the peer can be placed in the target, continue
//   - else if the peer can be placed in a parent, and there is no other
//     conflicting version shadowing it, continue
//   - else CONFLICT
// - If the peers are not in conflict, return the original node's value
//
// An exception to this logic is that if the target is the deepest location
// that a node can be placed, and the conflicting node can be placed deeper,
// then we will return REPLACE rather than CONFLICT, and Arborist will queue
// the replaced node for resolution elsewhere.

const localeCompare = require('@isaacs/string-locale-compare')('en')
const semver = require('semver')
const debug = require('./debug.js')
const peerEntrySets = require('./peer-entry-sets.js')
const deepestNestingTarget = require('./deepest-nesting-target.js')

const CONFLICT = Symbol('CONFLICT')
const OK = Symbol('OK')
const REPLACE = Symbol('REPLACE')
const KEEP = Symbol('KEEP')

class CanPlaceDep {
  // dep is a dep that we're trying to place.  it should already live in
  // a virtual tree where its peer set is loaded as children of the root.
  // target is the actual place where we're trying to place this dep
  // in a node_modules folder.
  // edge is the edge that we're trying to satisfy with this placement.
  // parent is the CanPlaceDep object of the entry node when placing a peer.
  constructor (options) {
    const {
      dep,
      target,
      edge,
      preferDedupe,
      parent = null,
      peerPath = [],
      explicitRequest = false,
    } = options

    debug(() => {
      if (!dep) {
        throw new Error('no dep provided to CanPlaceDep')
      }

      if (!target) {
        throw new Error('no target provided to CanPlaceDep')
      }

      if (!edge) {
        throw new Error('no edge provided to CanPlaceDep')
      }

      this._treeSnapshot = JSON.stringify([...target.root.inventory.entries()]
        .map(([loc, {packageName, version, resolved}]) => {
          return [loc, packageName, version, resolved]
        }).sort(([a], [b]) => localeCompare(a, b)))
    })

    // the result of whether we can place it or not
    this.canPlace = null
    // if peers conflict, but this one doesn't, then that is useful info
    this.canPlaceSelf = null

    this.dep = dep
    this.target = target
    this.edge = edge
    this.explicitRequest = explicitRequest

    // preventing cycles when we check peer sets
    this.peerPath = peerPath
    // we always prefer to dedupe peers, because they are trying
    // a bit harder to be singletons.
    this.preferDedupe = !!preferDedupe || edge.peer
    this.parent = parent
    this.children = []

    this.isSource = target === this.peerSetSource
    this.name = edge.name
    this.current = target.children.get(this.name)
    this.targetEdge = target.edgesOut.get(this.name)
    this.conflicts = new Map()

    // check if this dep was already subject to a peerDep override while
    // building the peerSet.
    this.edgeOverride = !dep.satisfies(edge)

    this.canPlace = this.checkCanPlace()
    if (!this.canPlaceSelf) {
      this.canPlaceSelf = this.canPlace
    }

    debug(() => {
      const treeSnapshot = JSON.stringify([...target.root.inventory.entries()]
        .map(([loc, {packageName, version, resolved}]) => {
          return [loc, packageName, version, resolved]
        }).sort(([a], [b]) => localeCompare(a, b)))
      /* istanbul ignore if */
      if (this._treeSnapshot !== treeSnapshot) {
        throw Object.assign(new Error('tree changed in CanPlaceDep'), {
          expect: this._treeSnapshot,
          actual: treeSnapshot,
        })
      }
    })
  }

  checkCanPlace () {
    const { target, targetEdge, current, dep } = this

    // if the dep failed to load, we're going to fail the build or
    // prune it out anyway, so just move forward placing/replacing it.
    if (dep.errors.length) {
      return current ? REPLACE : OK
    }

    // cannot place peers inside their dependents, except for tops
    if (targetEdge && targetEdge.peer && !target.isTop) {
      return CONFLICT
    }

    // skip this test if there's a current node, because we might be able
    // to dedupe against it anyway
    if (!current &&
        targetEdge &&
        !dep.satisfies(targetEdge) &&
        targetEdge !== this.edge) {
      return CONFLICT
    }

    return current ? this.checkCanPlaceCurrent() : this.checkCanPlaceNoCurrent()
  }

  // we know that the target has a dep by this name in its node_modules
  // already.  Can return KEEP, REPLACE, or CONFLICT.
  checkCanPlaceCurrent () {
    const { preferDedupe, explicitRequest, current, target, edge, dep } = this

    if (dep.matches(current)) {
      if (current.satisfies(edge) || this.edgeOverride) {
        return explicitRequest ? REPLACE : KEEP
      }
    }

    const { version: curVer } = current
    const { version: newVer } = dep
    const tryReplace = curVer && newVer && semver.gte(newVer, curVer)
    if (tryReplace && dep.canReplace(current)) {
      // It's extremely rare that a replaceable node would be a conflict, if
      // the current one wasn't a conflict, but it is theoretically possible
      // if peer deps are pinned.  In that case we treat it like any other
      // conflict, and keep trying.
      const cpp = this.canPlacePeers(REPLACE)
      if (cpp !== CONFLICT) {
        return cpp
      }
    }

    // ok, can't replace the current with new one, but maybe current is ok?
    if (current.satisfies(edge) && (!explicitRequest || preferDedupe)) {
      return KEEP
    }

    // if we prefer deduping, then try replacing newer with older
    if (preferDedupe && !tryReplace && dep.canReplace(current)) {
      const cpp = this.canPlacePeers(REPLACE)
      if (cpp !== CONFLICT) {
        return cpp
      }
    }

    // Check for interesting cases!
    // First, is this the deepest place that this thing can go, and NOT the
    // deepest place where the conflicting dep can go?  If so, replace it,
    // and let it re-resolve deeper in the tree.
    const myDeepest = this.deepestNestingTarget

    // ok, i COULD be placed deeper, so leave the current one alone.
    if (target !== myDeepest) {
      return CONFLICT
    }

    // if we are not checking a peerDep, then we MUST place it here, in the
    // target that has a non-peer dep on it.
    if (!edge.peer && target === edge.from) {
      return this.canPlacePeers(REPLACE)
    }

    // if we aren't placing a peer in a set, then we're done here.
    // This is ignored because it SHOULD be redundant, as far as I can tell,
    // with the deepest target and target===edge.from tests.  But until we
    // can prove that isn't possible, this condition is here for safety.
    /* istanbul ignore if - allegedly impossible */
    if (!this.parent && !edge.peer) {
      return CONFLICT
    }

    // check the deps in the peer group for each edge into that peer group
    // if ALL of them can be pushed deeper, or if it's ok to replace its
    // members with the contents of the new peer group, then we're good.
    let canReplace = true
    for (const [entryEdge, currentPeers] of peerEntrySets(current)) {
      if (entryEdge === this.edge || entryEdge === this.peerEntryEdge) {
        continue
      }

      // First, see if it's ok to just replace the peerSet entirely.
      // we do this by walking out from the entryEdge, because in a case like
      // this:
      //
      // v -> PEER(a@1||2)
      // a@1 -> PEER(b@1)
      // a@2 -> PEER(b@2)
      // b@1 -> PEER(a@1)
      // b@2 -> PEER(a@2)
      //
      // root
      // +-- v
      // +-- a@2
      // +-- b@2
      //
      // Trying to place a peer group of (a@1, b@1) would fail to note that
      // they can be replaced, if we did it by looping 1 by 1.  If we are
      // replacing something, we don't have to check its peer deps, because
      // the peerDeps in the placed peerSet will presumably satisfy.
      const entryNode = entryEdge.to
      const entryRep = dep.parent.children.get(entryNode.name)
      if (entryRep) {
        if (entryRep.canReplace(entryNode, dep.parent.children.keys())) {
          continue
        }
      }

      let canClobber = !entryRep
      if (!entryRep) {
        const peerReplacementWalk = new Set([entryNode])
        OUTER: for (const currentPeer of peerReplacementWalk) {
          for (const edge of currentPeer.edgesOut.values()) {
            if (!edge.peer || !edge.valid) {
              continue
            }
            const rep = dep.parent.children.get(edge.name)
            if (!rep) {
              if (edge.to) {
                peerReplacementWalk.add(edge.to)
              }
              continue
            }
            if (!rep.satisfies(edge)) {
              canClobber = false
              break OUTER
            }
          }
        }
      }
      if (canClobber) {
        continue
      }

      // ok, we can't replace, but maybe we can nest the current set deeper?
      let canNestCurrent = true
      for (const currentPeer of currentPeers) {
        if (!canNestCurrent) {
          break
        }

        // still possible to nest this peerSet
        const curDeep = deepestNestingTarget(entryEdge.from, currentPeer.name)
        if (curDeep === target || target.isDescendantOf(curDeep)) {
          canNestCurrent = false
          canReplace = false
        }
        if (canNestCurrent) {
          continue
        }
      }
    }

    // if we can nest or replace all the current peer groups, we can replace.
    if (canReplace) {
      return this.canPlacePeers(REPLACE)
    }

    return CONFLICT
  }

  checkCanPlaceNoCurrent () {
    const { target, peerEntryEdge, dep, name } = this

    // check to see what that name resolves to here, and who may depend on
    // being able to reach it by crawling up past the parent.  we know
    // that it's not the target's direct child node, and if it was a direct
    // dep of the target, we would have conflicted earlier.
    const current = target !== peerEntryEdge.from && target.resolve(name)
    if (current) {
      for (const edge of current.edgesIn.values()) {
        if (edge.from.isDescendantOf(target) && edge.valid) {
          if (!dep.satisfies(edge)) {
            return CONFLICT
          }
        }
      }
    }

    // no objections, so this is fine as long as peers are ok here.
    return this.canPlacePeers(OK)
  }

  get deepestNestingTarget () {
    const start = this.parent ? this.parent.deepestNestingTarget
      : this.edge.from
    return deepestNestingTarget(start, this.name)
  }

  get conflictChildren () {
    return this.allChildren.filter(c => c.canPlace === CONFLICT)
  }

  get allChildren () {
    const set = new Set(this.children)
    for (const child of set) {
      for (const grandchild of child.children) {
        set.add(grandchild)
      }
    }
    return [...set]
  }

  get top () {
    return this.parent ? this.parent.top : this
  }

  // check if peers can go here.  returns state or CONFLICT
  canPlacePeers (state) {
    this.canPlaceSelf = state
    if (this._canPlacePeers) {
      return this._canPlacePeers
    }

    // TODO: represent peerPath in ERESOLVE error somehow?
    const peerPath = [...this.peerPath, this.dep]
    let sawConflict = false
    for (const peerEdge of this.dep.edgesOut.values()) {
      if (!peerEdge.peer || !peerEdge.to || peerPath.includes(peerEdge.to)) {
        continue
      }
      const peer = peerEdge.to
      // it may be the case that the *initial* dep can be nested, but a peer
      // of that dep needs to be placed shallower, because the target has
      // a peer dep on the peer as well.
      const target = deepestNestingTarget(this.target, peer.name)
      const cpp = new CanPlaceDep({
        dep: peer,
        target,
        parent: this,
        edge: peerEdge,
        peerPath,
        // always place peers in preferDedupe mode
        preferDedupe: true,
      })
      /* istanbul ignore next */
      debug(() => {
        if (this.children.some(c => c.dep === cpp.dep)) {
          throw new Error('checking same dep repeatedly')
        }
      })
      this.children.push(cpp)

      if (cpp.canPlace === CONFLICT) {
        sawConflict = true
      }
    }

    this._canPlacePeers = sawConflict ? CONFLICT : state
    return this._canPlacePeers
  }

  // what is the node that is causing this peerSet to be placed?
  get peerSetSource () {
    return this.parent ? this.parent.peerSetSource : this.edge.from
  }

  get peerEntryEdge () {
    return this.top.edge
  }

  static get CONFLICT () {
    return CONFLICT
  }

  static get OK () {
    return OK
  }

  static get REPLACE () {
    return REPLACE
  }

  static get KEEP () {
    return KEEP
  }

  get description () {
    const { canPlace } = this
    return canPlace && canPlace.description ||
    /* istanbul ignore next - old node affordance */ canPlace
  }
}

module.exports = CanPlaceDep
