// Given a dep, a node that depends on it, and the edge representing that
// dependency, place the dep somewhere in the node's tree, and all of its
// peer dependencies.
//
// Handles all of the tree updating needed to place the dep, including
// removing replaced nodes, pruning now-extraneous or invalidated nodes,
// and saves a set of what was placed and what needs re-evaluation as
// a result.

const localeCompare = require('@isaacs/string-locale-compare')('en')
const log = require('proc-log')
const deepestNestingTarget = require('./deepest-nesting-target.js')
const CanPlaceDep = require('./can-place-dep.js')
const {
  KEEP,
  CONFLICT,
} = CanPlaceDep
const debug = require('./debug.js')

const Link = require('./link.js')
const gatherDepSet = require('./gather-dep-set.js')
const peerEntrySets = require('./peer-entry-sets.js')

class PlaceDep {
  constructor (options) {
    const {
      dep,
      edge,
      parent = null,
    } = options
    this.name = edge.name
    this.dep = dep
    this.edge = edge
    this.canPlace = null

    this.target = null
    this.placed = null

    // inherit all these fields from the parent to ensure consistency.
    const {
      preferDedupe,
      force,
      explicitRequest,
      updateNames,
      auditReport,
      legacyBundling,
      strictPeerDeps,
      legacyPeerDeps,
      globalStyle,
    } = parent || options
    Object.assign(this, {
      preferDedupe,
      force,
      explicitRequest,
      updateNames,
      auditReport,
      legacyBundling,
      strictPeerDeps,
      legacyPeerDeps,
      globalStyle,
    })

    this.children = []
    this.parent = parent
    this.peerConflict = null

    this.needEvaluation = new Set()

    this.checks = new Map()

    this.place()
  }

  place () {
    const {
      edge,
      dep,
      preferDedupe,
      globalStyle,
      legacyBundling,
      explicitRequest,
      updateNames,
      checks,
    } = this

    // nothing to do if the edge is fine as it is
    if (edge.to &&
        !edge.error &&
        !explicitRequest &&
        !updateNames.includes(edge.name) &&
        !this.isVulnerable(edge.to)) {
      return
    }

    // walk up the tree until we hit either a top/root node, or a place
    // where the dep is not a peer dep.
    const start = this.getStartNode()

    let canPlace = null
    let canPlaceSelf = null
    for (const target of start.ancestry()) {
      // if the current location has a peerDep on it, then we can't place here
      // this is pretty rare to hit, since we always prefer deduping peers,
      // and the getStartNode will start us out above any peers from the
      // thing that depends on it.  but we could hit it with something like:
      //
      // a -> (b@1, c@1)
      // +-- c@1
      // +-- b -> PEEROPTIONAL(v) (c@2)
      //     +-- c@2 -> (v)
      //
      // So we check if we can place v under c@2, that's fine.
      // Then we check under b, and can't, because of the optional peer dep.
      // but we CAN place it under a, so the correct thing to do is keep
      // walking up the tree.
      const targetEdge = target.edgesOut.get(edge.name)
      if (!target.isTop && targetEdge && targetEdge.peer) {
        continue
      }

      const cpd = new CanPlaceDep({
        dep,
        edge,
        // note: this sets the parent's canPlace as the parent of this
        // canPlace, but it does NOT add this canPlace to the parent's
        // children.  This way, we can know that it's a peer dep, and
        // get the top edge easily, while still maintaining the
        // tree of checks that factored into the original decision.
        parent: this.parent && this.parent.canPlace,
        target,
        preferDedupe,
        explicitRequest: this.explicitRequest,
      })
      checks.set(target, cpd)

      // It's possible that a "conflict" is a conflict among the *peers* of
      // a given node we're trying to place, but there actually is no current
      // node.  Eg,
      // root -> (a, b)
      // a -> PEER(c)
      // b -> PEER(d)
      // d -> PEER(c@2)
      // We place (a), and get a peer of (c) along with it.
      // then we try to place (b), and get CONFLICT in the check, because
      // of the conflicting peer from (b)->(d)->(c@2).  In that case, we
      // should treat (b) and (d) as OK, and place them in the last place
      // where they did not themselves conflict, and skip c@2 if conflict
      // is ok by virtue of being forced or not ours and not strict.
      if (cpd.canPlaceSelf !== CONFLICT) {
        canPlaceSelf = cpd
      }

      // we found a place this can go, along with all its peer friends.
      // we break when we get the first conflict
      if (cpd.canPlace !== CONFLICT) {
        canPlace = cpd
      } else {
        break
      }

      // if it's a load failure, just plop it in the first place attempted,
      // since we're going to crash the build or prune it out anyway.
      // but, this will frequently NOT be a successful canPlace, because
      // it'll have no version or other information.
      if (dep.errors.length) {
        break
      }

      // nest packages like npm v1 and v2
      // very disk-inefficient
      if (legacyBundling) {
        break
      }

      // when installing globally, or just in global style, we never place
      // deps above the first level.
      if (globalStyle) {
        const rp = target.resolveParent
        if (rp && rp.isProjectRoot) {
          break
        }
      }
    }

    Object.assign(this, {
      canPlace,
      canPlaceSelf,
    })
    this.current = edge.to

    // if we can't find a target, that means that the last place checked,
    // and all the places before it, had a conflict.
    if (!canPlace) {
      // if not forced, or it's our dep, or strictPeerDeps is set, then
      // this is an ERESOLVE error.
      if (!this.conflictOk) {
        return this.failPeerConflict()
      }

      // ok!  we're gonna allow the conflict, but we should still warn
      // if we have a current, then we treat CONFLICT as a KEEP.
      // otherwise, we just skip it.  Only warn on the one that actually
      // could not be placed somewhere.
      if (!canPlaceSelf) {
        this.warnPeerConflict()
        return
      }

      this.canPlace = canPlaceSelf
    }

    // now we have a target, a tree of CanPlaceDep results for the peer group,
    // and we are ready to go
    this.placeInTree()
  }

  placeInTree () {
    const {
      dep,
      canPlace,
      edge,
    } = this

    /* istanbul ignore next */
    if (!canPlace) {
      debug(() => {
        throw new Error('canPlace not set, but trying to place in tree')
      })
      return
    }

    const { target } = canPlace

    log.silly(
      'placeDep',
      target.location || 'ROOT',
      `${dep.name}@${dep.version}`,
      canPlace.description,
      `for: ${this.edge.from.package._id || this.edge.from.location}`,
      `want: ${edge.spec || '*'}`
    )

    const placementType = canPlace.canPlace === CONFLICT
      ? canPlace.canPlaceSelf
      : canPlace.canPlace

    // if we're placing in the tree with --force, we can get here even though
    // it's a conflict.  Treat it as a KEEP, but warn and move on.
    if (placementType === KEEP) {
      // this was an overridden peer dep
      if (edge.peer && !edge.valid) {
        this.warnPeerConflict()
      }

      // if we get a KEEP in a update scenario, then we MAY have something
      // already duplicating this unnecessarily!  For example:
      // ```
      // root (dep: y@1)
      // +-- x (dep: y@1.1)
      // |   +-- y@1.1.0 (replacing with 1.1.2, got KEEP at the root)
      // +-- y@1.1.2 (updated already from 1.0.0)
      // ```
      // Now say we do `reify({update:['y']})`, and the latest version is
      // 1.1.2, which we now have in the root.  We'll try to place y@1.1.2
      // first in x, then in the root, ending with KEEP, because we already
      // have it.  In that case, we ought to REMOVE the nm/x/nm/y node, because
      // it is an unnecessary duplicate.
      this.pruneDedupable(target)
      return
    }

    // we were told to place it here in the target, so either it does not
    // already exist in the tree, OR it's shadowed.
    // handle otherwise unresolvable dependency nesting loops by
    // creating a symbolic link
    // a1 -> b1 -> a2 -> b2 -> a1 -> ...
    // instead of nesting forever, when the loop occurs, create
    // a symbolic link to the earlier instance
    for (let p = target; p; p = p.resolveParent) {
      if (p.matches(dep) && !p.isTop) {
        this.placed = new Link({ parent: target, target: p })
        return
      }
    }

    // XXX if we are replacing SOME of a peer entry group, we will need to
    // remove any that are not being replaced and will now be invalid, and
    // re-evaluate them deeper into the tree.

    const virtualRoot = dep.parent
    this.placed = new dep.constructor({
      name: dep.name,
      pkg: dep.package,
      resolved: dep.resolved,
      integrity: dep.integrity,
      legacyPeerDeps: this.legacyPeerDeps,
      error: dep.errors[0],
      ...(dep.isLink ? { target: dep.target, realpath: dep.realpath } : {}),
    })

    this.oldDep = target.children.get(this.name)
    if (this.oldDep) {
      this.replaceOldDep()
    } else {
      this.placed.parent = target
    }

    // if it's an overridden peer dep, warn about it
    if (edge.peer && !this.placed.satisfies(edge)) {
      this.warnPeerConflict()
    }

    // If the edge is not an error, then we're updating something, and
    // MAY end up putting a better/identical node further up the tree in
    // a way that causes an unnecessary duplication.  If so, remove the
    // now-unnecessary node.
    if (edge.valid && edge.to && edge.to !== this.placed) {
      this.pruneDedupable(edge.to, false)
    }

    // in case we just made some duplicates that can be removed,
    // prune anything deeper in the tree that can be replaced by this
    for (const node of target.root.inventory.query('name', this.name)) {
      if (node.isDescendantOf(target) && !node.isTop) {
        this.pruneDedupable(node, false)
        // only walk the direct children of the ones we kept
        if (node.root === target.root) {
          for (const kid of node.children.values()) {
            this.pruneDedupable(kid, false)
          }
        }
      }
    }

    // also place its unmet or invalid peer deps at this location
    // loop through any peer deps from the thing we just placed, and place
    // those ones as well.  it's safe to do this with the virtual nodes,
    // because we're copying rather than moving them out of the virtual root,
    // otherwise they'd be gone and the peer set would change throughout
    // this loop.
    for (const peerEdge of this.placed.edgesOut.values()) {
      if (peerEdge.valid || !peerEdge.peer || peerEdge.overridden) {
        continue
      }

      const peer = virtualRoot.children.get(peerEdge.name)

      // Note: if the virtualRoot *doesn't* have the peer, then that means
      // it's an optional peer dep.  If it's not being properly met (ie,
      // peerEdge.valid is false), then this is likely heading for an
      // ERESOLVE error, unless it can walk further up the tree.
      if (!peer) {
        continue
      }

      // overridden peerEdge, just accept what's there already
      if (!peer.satisfies(peerEdge)) {
        continue
      }

      this.children.push(new PlaceDep({
        parent: this,
        dep: peer,
        node: this.placed,
        edge: peerEdge,
      }))
    }
  }

  replaceOldDep () {
    const target = this.oldDep.parent

    // XXX handle replacing an entire peer group?
    // what about cases where we need to push some other peer groups deeper
    // into the tree?  all the tree updating should be done here, and track
    // all the things that we add and remove, so that we can know what
    // to re-evaluate.

    // if we're replacing, we should also remove any nodes for edges that
    // are now invalid, and where this (or its deps) is the only dependent,
    // and also recurse on that pruning.  Otherwise leaving that dep node
    // around can result in spurious conflicts pushing nodes deeper into
    // the tree than needed in the case of cycles that will be removed
    // later anyway.
    const oldDeps = []
    for (const [name, edge] of this.oldDep.edgesOut.entries()) {
      if (!this.placed.edgesOut.has(name) && edge.to) {
        oldDeps.push(...gatherDepSet([edge.to], e => e.to !== edge.to))
      }
    }

    // gather all peer edgesIn which are at this level, and will not be
    // satisfied by the new dependency.  Those are the peer sets that need
    // to be either warned about (if they cannot go deeper), or removed and
    // re-placed (if they can).
    const prunePeerSets = []
    for (const edge of this.oldDep.edgesIn) {
      if (this.placed.satisfies(edge) ||
          !edge.peer ||
          edge.from.parent !== target ||
          edge.overridden) {
        // not a peer dep, not invalid, or not from this level, so it's fine
        // to just let it re-evaluate as a problemEdge later, or let it be
        // satisfied by the new dep being placed.
        continue
      }
      for (const entryEdge of peerEntrySets(edge.from).keys()) {
        // either this one needs to be pruned and re-evaluated, or marked
        // as overridden and warned about.  If the entryEdge comes in from
        // the root, then we have to leave it alone, and in that case, it
        // will have already warned or crashed by getting to this point.
        const entryNode = entryEdge.to
        const deepestTarget = deepestNestingTarget(entryNode)
        if (deepestTarget !== target && !entryEdge.from.isRoot) {
          prunePeerSets.push(...gatherDepSet([entryNode], e => {
            return e.to !== entryNode && !e.overridden
          }))
        } else {
          this.warnPeerConflict(edge, this.dep)
        }
      }
    }

    this.placed.replace(this.oldDep)
    this.pruneForReplacement(this.placed, oldDeps)
    for (const dep of prunePeerSets) {
      for (const edge of dep.edgesIn) {
        this.needEvaluation.add(edge.from)
      }
      dep.root = null
    }
  }

  pruneForReplacement (node, oldDeps) {
    // gather up all the now-invalid/extraneous edgesOut, as long as they are
    // only depended upon by the old node/deps
    const invalidDeps = new Set([...node.edgesOut.values()]
      .filter(e => e.to && !e.valid).map(e => e.to))
    for (const dep of oldDeps) {
      const set = gatherDepSet([dep], e => e.to !== dep && e.valid)
      for (const dep of set) {
        invalidDeps.add(dep)
      }
    }

    // ignore dependency edges from the node being replaced, but
    // otherwise filter the set down to just the set with no
    // dependencies from outside the set, except the node in question.
    const deps = gatherDepSet(invalidDeps, edge =>
      edge.from !== node && edge.to !== node && edge.valid)

    // now just delete whatever's left, because it's junk
    for (const dep of deps) {
      dep.root = null
    }
  }

  // prune all the nodes in a branch of the tree that can be safely removed
  // This is only the most basic duplication detection; it finds if there
  // is another satisfying node further up the tree, and if so, dedupes.
  // Even in legacyBundling mode, we do this amount of deduplication.
  pruneDedupable (node, descend = true) {
    if (node.canDedupe(this.preferDedupe)) {
      // gather up all deps that have no valid edges in from outside
      // the dep set, except for this node we're deduping, so that we
      // also prune deps that would be made extraneous.
      const deps = gatherDepSet([node], e => e.to !== node && e.valid)
      for (const node of deps) {
        node.root = null
      }
      return
    }
    if (descend) {
      // sort these so that they're deterministically ordered
      // otherwise, resulting tree shape is dependent on the order
      // in which they happened to be resolved.
      const nodeSort = (a, b) => localeCompare(a.location, b.location)

      const children = [...node.children.values()].sort(nodeSort)
      for (const child of children) {
        this.pruneDedupable(child)
      }
      const fsChildren = [...node.fsChildren].sort(nodeSort)
      for (const topNode of fsChildren) {
        const children = [...topNode.children.values()].sort(nodeSort)
        for (const child of children) {
          this.pruneDedupable(child)
        }
      }
    }
  }

  get conflictOk () {
    return this.force || (!this.isMine && !this.strictPeerDeps)
  }

  get isMine () {
    const { edge } = this.top
    const { from: node } = edge

    if (node.isWorkspace || node.isProjectRoot) {
      return true
    }

    if (!edge.peer) {
      return false
    }

    // re-entry case.  check if any non-peer edges come from the project,
    // or any entryEdges on peer groups are from the root.
    let hasPeerEdges = false
    for (const edge of node.edgesIn) {
      if (edge.peer) {
        hasPeerEdges = true
        continue
      }
      if (edge.from.isWorkspace || edge.from.isProjectRoot) {
        return true
      }
    }
    if (hasPeerEdges) {
      for (const edge of peerEntrySets(node).keys()) {
        if (edge.from.isWorkspace || edge.from.isProjectRoot) {
          return true
        }
      }
    }

    return false
  }

  warnPeerConflict (edge, dep) {
    edge = edge || this.edge
    dep = dep || this.dep
    edge.overridden = true
    const expl = this.explainPeerConflict(edge, dep)
    log.warn('ERESOLVE', 'overriding peer dependency', expl)
  }

  failPeerConflict (edge, dep) {
    edge = edge || this.top.edge
    dep = dep || this.top.dep
    const expl = this.explainPeerConflict(edge, dep)
    throw Object.assign(new Error('could not resolve'), expl)
  }

  explainPeerConflict (edge, dep) {
    const { from: node } = edge
    const curNode = node.resolve(edge.name)

    const expl = {
      code: 'ERESOLVE',
      edge: edge.explain(),
      dep: dep.explain(edge),
    }

    if (this.parent) {
      // this is the conflicted peer
      expl.current = curNode && curNode.explain(edge)
      expl.peerConflict = this.current && this.current.explain(this.edge)
    } else {
      expl.current = curNode && curNode.explain()
      if (this.canPlaceSelf && this.canPlaceSelf.canPlaceSelf !== CONFLICT) {
        // failed while checking for a child dep
        const cps = this.canPlaceSelf
        for (const peer of cps.conflictChildren) {
          if (peer.current) {
            expl.peerConflict = {
              current: peer.current.explain(),
              peer: peer.dep.explain(peer.edge),
            }
            break
          }
        }
      } else {
        expl.peerConflict = {
          current: this.current && this.current.explain(),
          peer: this.dep.explain(this.edge),
        }
      }
    }

    const {
      strictPeerDeps,
      force,
      isMine,
    } = this
    Object.assign(expl, {
      strictPeerDeps,
      force,
      isMine,
    })

    // XXX decorate more with this.canPlace and this.canPlaceSelf,
    // this.checks, this.children, walk over conflicted peers, etc.
    return expl
  }

  getStartNode () {
    // if we are a peer, then we MUST be at least as shallow as the
    // peer dependent
    const from = this.parent ? this.parent.getStartNode() : this.edge.from
    return deepestNestingTarget(from, this.name)
  }

  get top () {
    return this.parent ? this.parent.top : this
  }

  isVulnerable (node) {
    return this.auditReport && this.auditReport.isVulnerable(node)
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
}

module.exports = PlaceDep
