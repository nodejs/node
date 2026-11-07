// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (c) 2002-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/
//
//  rbbitblb.cpp
//

#include <functional>

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/unistr.h"
#include "rbbitblb.h"
#include "rbbirb.h"
#include "rbbiscan.h"
#include "rbbisetb.h"
#include "rbbidata.h"
#include "cstring.h"
#include "uassert.h"
#include "uvectr32.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

namespace {

// Given the `RBBITableBuilder::fDStates` vector of `RBBIStateDescriptor`s, returns
// true if a state for which `isSink` returns true is reachable from state `source` by following
// transitions without going through any state for which `excludedState` returns true.
// Note that “going through” is distinct from “reaching”, and means entering and then leaving: A
// state can be both a sink and excluded, and can still be reachable.
bool reachableByTransitions(const UVector &states, const int32_t source,
                            const std::function<bool(int32_t)> isSink,
                            const std::function<bool(int32_t)> excludedState, UErrorCode &status) {
    UStack boundary(status);
    {
        UVector32 &transitionsFromSource =
            *static_cast<RBBIStateDescriptor *>(states.elementAt(source))->fDtran;
        // We do not initialize boundary to `{source}`, but instead to the set of states one
        // transition away from `source`; if `source` sets lookahead l and accepts lookahead k, we
        // only need k and l to occupy distinct slots if there is a `source`-to-`source` path.
        for (int32_t symbol = 0; symbol < transitionsFromSource.size(); ++symbol) {
            const int32_t state = transitionsFromSource.elementAti(symbol);
            if (state != 0) {
                boundary.push(state, status);
            }
        }
    }
    const auto visited = prv::make_unique<bool[]>(states.size(), status);
    while (U_SUCCESS(status) && !boundary.empty()) {
        const int32_t s = boundary.popi();
        if (isSink(s)) {
            return true;
        }
        if (excludedState(s) || visited[s]) {
            continue;
        }
        visited[s] = true;
        UVector32 &transitions = *static_cast<RBBIStateDescriptor *>(states.elementAt(s))->fDtran;
        for (int32_t symbol = 0; symbol < transitions.size(); ++symbol) {
            const int32_t t = transitions.elementAti(symbol);
            if (t != 0 && !visited[t]) {
                boundary.push(t, status);
            }
        }
    }
    return false;
}

} // namespace

const int32_t kMaxStateFor8BitsTable = 255;

RBBITableBuilder::RBBITableBuilder(RBBIRuleBuilder *rb, RBBINode **rootNode, UErrorCode &status) :
        fRB(rb),
        fTree(*rootNode),
        fStatus(&status),
        fDStates(nullptr),
        fSafeTable(nullptr) {
    if (U_FAILURE(status)) {
        return;
    }
    // fDStates is UVector<RBBIStateDescriptor *>
    fDStates = new UVector(status);
    if (U_SUCCESS(status) && fDStates == nullptr ) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}



RBBITableBuilder::~RBBITableBuilder() {
    int i;
    for (i=0; i<fDStates->size(); i++) {
        delete static_cast<RBBIStateDescriptor*>(fDStates->elementAt(i));
    }
    delete fDStates;
    delete fSafeTable;
    delete fLookAheadRuleMap;
}


//-----------------------------------------------------------------------------
//
//   RBBITableBuilder::buildForwardTable  -  This is the main function for building
//                               the DFA state transition table from the RBBI rules parse tree.
//
//-----------------------------------------------------------------------------
void  RBBITableBuilder::buildForwardTable() {

    if (U_FAILURE(*fStatus)) {
        return;
    }

    // If there were no rules, just return.  This situation can easily arise
    //   for the reverse rules.
    if (fTree==nullptr) {
        return;
    }

    //
    // Walk through the tree, replacing any references to $variables with a copy of the
    //   parse tree for the substitution expression.
    //
    fTree = fTree->flattenVariables(*fStatus, 0);
    if (U_FAILURE(*fStatus)) {
        return;
    }
#ifdef RBBI_DEBUG
    if (fRB->fDebugEnv && uprv_strstr(fRB->fDebugEnv, "ftree")) {
        RBBIDebugPuts("\nParse tree after flattening variable references.");
        RBBINode::printTree(fTree, true);
    }
#endif

    //
    // Add a unique right-end marker to the expression.
    //   Appears as a cat-node, left child being the original tree,
    //   right child being the end marker.
    //
    RBBINode *cn = new RBBINode(RBBINode::opCat, *fStatus);
    // Exit if memory allocation failed.
    if (cn == nullptr) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(*fStatus)) {
        delete cn;
        return;
    }
    cn->fLeftChild = fTree;
    fTree->fParent = cn;
    RBBINode *endMarkerNode = cn->fRightChild = new RBBINode(RBBINode::endMark, *fStatus);
    // Delete and exit if memory allocation failed.
    if (cn->fRightChild == nullptr) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(*fStatus)) {
        delete cn;
        return;
    }
    cn->fRightChild->fParent = cn;
    fTree = cn;

    //
    //  Replace all references to UnicodeSets with the tree for the equivalent
    //      expression.
    //
    fTree->flattenSets(*fStatus, 0);
    if (U_FAILURE(*fStatus)) {
        return;
    }
#ifdef RBBI_DEBUG
    if (fRB->fDebugEnv && uprv_strstr(fRB->fDebugEnv, "stree")) {
        RBBIDebugPuts("\nParse tree after flattening Unicode Set references.");
        RBBINode::printTree(fTree, true);
    }
#endif


    //
    // calculate the functions nullable, firstpos, lastpos and followpos on
    // nodes in the parse tree.
    //    See the algorithm description in Aho.
    //    Understanding how this works by looking at the code alone will be
    //       nearly impossible.
    //
    calcNullable(fTree);
    calcFirstPos(fTree);
    calcLastPos(fTree);
    calcFollowPos(fTree);
    if (fRB->fDebugEnv && uprv_strstr(fRB->fDebugEnv, "pos")) {
        RBBIDebugPuts("\n");
        printPosSets(fTree);
    }

    //
    //  For "chained" rules, modify the followPos sets
    //
    if (fRB->fChainRules) {
        calcChainedFollowPos(fTree, endMarkerNode);
    }

    //
    // Build the DFA state transition tables.
    //
    buildStateTable();
    mapLookAheadRules();
    flagAcceptingStates();
    flagLookAheadStates();
    flagTaggedStates();

    //
    // Update the global table of rule status {tag} values
    // The rule builder has a global vector of status values that are common
    //    for all tables.  Merge the ones from this table into the global set.
    //
    mergeRuleStatusVals();
}



//-----------------------------------------------------------------------------
//
//   calcNullable.    Impossible to explain succinctly.  See Aho, section 3.9
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::calcNullable(RBBINode *n) {
    if (n == nullptr) {
        return;
    }
    if (n->fType == RBBINode::setRef ||
        n->fType == RBBINode::endMark ) {
        // These are non-empty leaf node types.
        n->fNullable = false;
        return;
    }

    if (n->fType == RBBINode::lookAhead || n->fType == RBBINode::tag) {
        // Lookahead marker node.  It's a leaf, so no recursion on children.
        // It's nullable because it does not match any literal text from the input stream.
        n->fNullable = true;
        return;
    }


    // The node is not a leaf.
    //  Calculate nullable on its children.
    calcNullable(n->fLeftChild);
    calcNullable(n->fRightChild);

    // Apply functions from table 3.40 in Aho
    if (n->fType == RBBINode::opOr) {
        n->fNullable = n->fLeftChild->fNullable || n->fRightChild->fNullable;
    }
    else if (n->fType == RBBINode::opCat) {
        n->fNullable = n->fLeftChild->fNullable && n->fRightChild->fNullable;
    }
    else if (n->fType == RBBINode::opStar || n->fType == RBBINode::opQuestion) {
        n->fNullable = true;
    }
    else {
        n->fNullable = false;
    }
}




//-----------------------------------------------------------------------------
//
//   calcFirstPos.    Impossible to explain succinctly.  See Aho, section 3.9
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::calcFirstPos(RBBINode *n) {
    if (n == nullptr) {
        return;
    }
    if (n->fType == RBBINode::leafChar  ||
        n->fType == RBBINode::endMark   ||
        n->fType == RBBINode::lookAhead ||
        n->fType == RBBINode::tag) {
        // These are non-empty leaf node types.
        // Note: In order to maintain the sort invariant on the set,
        // this function should only be called on a node whose set is
        // empty to start with.
        n->fFirstPosSet->addElement(n, *fStatus);
        return;
    }

    // The node is not a leaf.
    //  Calculate firstPos on its children.
    calcFirstPos(n->fLeftChild);
    calcFirstPos(n->fRightChild);

    // Apply functions from table 3.40 in Aho
    if (n->fType == RBBINode::opOr) {
        setAdd(n->fFirstPosSet, n->fLeftChild->fFirstPosSet);
        setAdd(n->fFirstPosSet, n->fRightChild->fFirstPosSet);
    }
    else if (n->fType == RBBINode::opCat) {
        setAdd(n->fFirstPosSet, n->fLeftChild->fFirstPosSet);
        if (n->fLeftChild->fNullable) {
            setAdd(n->fFirstPosSet, n->fRightChild->fFirstPosSet);
        }
    }
    else if (n->fType == RBBINode::opStar ||
             n->fType == RBBINode::opQuestion ||
             n->fType == RBBINode::opPlus) {
        setAdd(n->fFirstPosSet, n->fLeftChild->fFirstPosSet);
    }
}



//-----------------------------------------------------------------------------
//
//   calcLastPos.    Impossible to explain succinctly.  See Aho, section 3.9
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::calcLastPos(RBBINode *n) {
    if (n == nullptr) {
        return;
    }
    if (n->fType == RBBINode::leafChar  ||
        n->fType == RBBINode::endMark   ||
        n->fType == RBBINode::lookAhead ||
        n->fType == RBBINode::tag) {
        // These are non-empty leaf node types.
        // Note: In order to maintain the sort invariant on the set,
        // this function should only be called on a node whose set is
        // empty to start with.
        n->fLastPosSet->addElement(n, *fStatus);
        return;
    }

    // The node is not a leaf.
    //  Calculate lastPos on its children.
    calcLastPos(n->fLeftChild);
    calcLastPos(n->fRightChild);

    // Apply functions from table 3.40 in Aho
    if (n->fType == RBBINode::opOr) {
        setAdd(n->fLastPosSet, n->fLeftChild->fLastPosSet);
        setAdd(n->fLastPosSet, n->fRightChild->fLastPosSet);
    }
    else if (n->fType == RBBINode::opCat) {
        setAdd(n->fLastPosSet, n->fRightChild->fLastPosSet);
        if (n->fRightChild->fNullable) {
            setAdd(n->fLastPosSet, n->fLeftChild->fLastPosSet);
        }
    }
    else if (n->fType == RBBINode::opStar     ||
             n->fType == RBBINode::opQuestion ||
             n->fType == RBBINode::opPlus) {
        setAdd(n->fLastPosSet, n->fLeftChild->fLastPosSet);
    }
}



//-----------------------------------------------------------------------------
//
//   calcFollowPos.    Impossible to explain succinctly.  See Aho, section 3.9
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::calcFollowPos(RBBINode *n) {
    if (n == nullptr ||
        n->fType == RBBINode::leafChar ||
        n->fType == RBBINode::endMark) {
        return;
    }

    calcFollowPos(n->fLeftChild);
    calcFollowPos(n->fRightChild);

    // Aho rule #1
    if (n->fType == RBBINode::opCat) {
        RBBINode *i;   // is 'i' in Aho's description
        uint32_t     ix;

        UVector *LastPosOfLeftChild = n->fLeftChild->fLastPosSet;

        for (ix = 0; ix < static_cast<uint32_t>(LastPosOfLeftChild->size()); ix++) {
            i = static_cast<RBBINode*>(LastPosOfLeftChild->elementAt(ix));
            setAdd(i->fFollowPos, n->fRightChild->fFirstPosSet);
        }
    }

    // Aho rule #2
    if (n->fType == RBBINode::opStar ||
        n->fType == RBBINode::opPlus) {
        RBBINode   *i;  // again, n and i are the names from Aho's description.
        uint32_t    ix;

        for (ix = 0; ix < static_cast<uint32_t>(n->fLastPosSet->size()); ix++) {
            i = static_cast<RBBINode*>(n->fLastPosSet->elementAt(ix));
            setAdd(i->fFollowPos, n->fFirstPosSet);
        }
    }



}

//-----------------------------------------------------------------------------
//
//    addRuleRootNodes    Recursively walk a parse tree, adding all nodes flagged
//                        as roots of a rule to a destination vector.
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::addRuleRootNodes(UVector *dest, RBBINode *node) {
    if (node == nullptr || U_FAILURE(*fStatus)) {
        return;
    }
    U_ASSERT(!dest->hasDeleter());
    if (node->fRuleRoot) {
        dest->addElement(node, *fStatus);
        // Note: rules cannot nest. If we found a rule start node,
        //       no child node can also be a start node.
        return;
    }
    addRuleRootNodes(dest, node->fLeftChild);
    addRuleRootNodes(dest, node->fRightChild);
}

//-----------------------------------------------------------------------------
//
//   calcChainedFollowPos.    Modify the previously calculated followPos sets
//                            to implement rule chaining.  NOT described by Aho
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::calcChainedFollowPos(RBBINode *tree, RBBINode *endMarkNode) {

    UVector         leafNodes(*fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }

    // get a list all leaf nodes
    tree->findNodes(&leafNodes, RBBINode::leafChar, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }

    // Collect all leaf nodes that can start matches for rules
    // with inbound chaining enabled, which is the union of the 
    // firstPosition sets from each of the rule root nodes.
    
    UVector ruleRootNodes(*fStatus);
    addRuleRootNodes(&ruleRootNodes, tree);

    UVector matchStartNodes(*fStatus);
    for (int j=0; j<ruleRootNodes.size(); ++j) {
        RBBINode *node = static_cast<RBBINode *>(ruleRootNodes.elementAt(j));
        if (node->fChainIn) {
            setAdd(&matchStartNodes, node->fFirstPosSet);
        }
    }
    if (U_FAILURE(*fStatus)) {
        return;
    }

    int32_t  endNodeIx;
    int32_t  startNodeIx;

    for (endNodeIx=0; endNodeIx<leafNodes.size(); endNodeIx++) {
        RBBINode* endNode = static_cast<RBBINode*>(leafNodes.elementAt(endNodeIx));

        // Identify leaf nodes that correspond to overall rule match positions.
        // These include the endMarkNode in their followPos sets.
        //
        // Note: do not consider other end marker nodes, those that are added to
        //       look-ahead rules. These can't chain; a match immediately stops
        //       further matching. This leaves exactly one end marker node, the one
        //       at the end of the complete tree.

        if (!endNode->fFollowPos->contains(endMarkNode)) {
            continue;
        }

        // We've got a node that can end a match.

        // Now iterate over the nodes that can start a match, looking for ones
        //   with the same char class as our ending node.
        RBBINode *startNode;
        for (startNodeIx = 0; startNodeIx<matchStartNodes.size(); startNodeIx++) {
            startNode = static_cast<RBBINode*>(matchStartNodes.elementAt(startNodeIx));
            if (startNode->fType != RBBINode::leafChar) {
                continue;
            }

            if (endNode->fVal == startNode->fVal) {
                // The end val (character class) of one possible match is the
                //   same as the start of another.

                // Add all nodes from the followPos of the start node to the
                //  followPos set of the end node, which will have the effect of
                //  letting matches transition from a match state at endNode
                //  to the second char of a match starting with startNode.
                setAdd(endNode->fFollowPos, startNode->fFollowPos);
            }
        }
    }
}


//-----------------------------------------------------------------------------
//
//   bofFixup.    Fixup for state tables that include {bof} beginning of input testing.
//                Do an swizzle similar to chaining, modifying the followPos set of
//                the bofNode to include the followPos nodes from other {bot} nodes
//                scattered through the tree.
//
//                This function has much in common with calcChainedFollowPos().
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::bofFixup() {

    if (U_FAILURE(*fStatus)) {
        return;
    }

    //   The parse tree looks like this ...
    //         fTree root  --->       <cat>
    //                               /     \       .
    //                            <cat>   <#end node>
    //                           /     \  .
    //                     <bofNode>   rest
    //                               of tree
    //
    //    We will be adding things to the followPos set of the <bofNode>
    //
    RBBINode  *bofNode = fTree->fLeftChild->fLeftChild;
    U_ASSERT(bofNode->fType == RBBINode::leafChar);
    U_ASSERT(bofNode->fVal == 2);

    // Get all nodes that can be the start a match of the user-written rules
    //  (excluding the fake bofNode)
    //  We want the nodes that can start a match in the
    //     part labeled "rest of tree"
    // 
    UVector *matchStartNodes = fTree->fLeftChild->fRightChild->fFirstPosSet;

    RBBINode *startNode;
    int       startNodeIx;
    for (startNodeIx = 0; startNodeIx<matchStartNodes->size(); startNodeIx++) {
        startNode = static_cast<RBBINode*>(matchStartNodes->elementAt(startNodeIx));
        if (startNode->fType != RBBINode::leafChar) {
            continue;
        }

        if (startNode->fVal == bofNode->fVal) {
            //  We found a leaf node corresponding to a {bof} that was
            //    explicitly written into a rule.
            //  Add everything from the followPos set of this node to the
            //    followPos set of the fake bofNode at the start of the tree.
            //  
            setAdd(bofNode->fFollowPos, startNode->fFollowPos);
        }
    }
}

//-----------------------------------------------------------------------------
//
//   buildStateTable()    Determine the set of runtime DFA states and the
//                        transition tables for these states, by the algorithm
//                        of fig. 3.44 in Aho.
//
//                        Most of the comments are quotes of Aho's pseudo-code.
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::buildStateTable() {
    if (U_FAILURE(*fStatus)) {
        return;
    }
    RBBIStateDescriptor *failState;
    // Set it to nullptr to avoid uninitialized warning
    RBBIStateDescriptor *initialState = nullptr;
    //
    // Add a dummy state 0 - the stop state.  Not from Aho.
    int      lastInputSymbol = fRB->fSetBuilder->getNumCharCategories() - 1;
    failState = new RBBIStateDescriptor(lastInputSymbol, fStatus);
    if (failState == nullptr) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
        goto ExitBuildSTdeleteall;
    }
    failState->fPositions = new UVector(*fStatus);
    if (failState->fPositions == nullptr) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
    }
    if (failState->fPositions == nullptr || U_FAILURE(*fStatus)) {
        goto ExitBuildSTdeleteall;
    }
    fDStates->addElement(failState, *fStatus);
    if (U_FAILURE(*fStatus)) {
        goto ExitBuildSTdeleteall;
    }

    // initially, the only unmarked state in Dstates is firstpos(root),
    //       where toot is the root of the syntax tree for (r)#;
    initialState = new RBBIStateDescriptor(lastInputSymbol, fStatus);
    if (initialState == nullptr) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(*fStatus)) {
        goto ExitBuildSTdeleteall;
    }
    initialState->fPositions = new UVector(*fStatus);
    if (initialState->fPositions == nullptr) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(*fStatus)) {
        goto ExitBuildSTdeleteall;
    }
    setAdd(initialState->fPositions, fTree->fFirstPosSet);
    fDStates->addElement(initialState, *fStatus);
    if (U_FAILURE(*fStatus)) {
        goto ExitBuildSTdeleteall;
    }

    // while there is an unmarked state T in Dstates do begin
    for (;;) {
        RBBIStateDescriptor *T = nullptr;
        int32_t              tx;
        for (tx=1; tx<fDStates->size(); tx++) {
            RBBIStateDescriptor *temp;
            temp = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(tx));
            if (temp->fMarked == false) {
                T = temp;
                break;
            }
        }
        if (T == nullptr) {
            break;
        }

        // mark T;
        T->fMarked = true;

        // for each input symbol a do begin
        int32_t  a;
        for (a = 1; a<=lastInputSymbol; a++) {
            // let U be the set of positions that are in followpos(p)
            //    for some position p in T
            //    such that the symbol at position p is a;
            UVector    *U = nullptr;
            RBBINode   *p;
            int32_t     px;
            for (px=0; px<T->fPositions->size(); px++) {
                p = static_cast<RBBINode*>(T->fPositions->elementAt(px));
                if ((p->fType == RBBINode::leafChar) &&  (p->fVal == a)) {
                    if (U == nullptr) {
                        U = new UVector(*fStatus);
                        if (U == nullptr) {
                        	*fStatus = U_MEMORY_ALLOCATION_ERROR;
                        	goto ExitBuildSTdeleteall;
                        }
                    }
                    setAdd(U, p->fFollowPos);
                }
            }

            // if U is not empty and not in DStates then
            int32_t  ux = 0;
            UBool    UinDstates = false;
            if (U != nullptr) {
                U_ASSERT(U->size() > 0);
                int  ix;
                for (ix=0; ix<fDStates->size(); ix++) {
                    RBBIStateDescriptor *temp2;
                    temp2 = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(ix));
                    if (setEquals(U, temp2->fPositions)) {
                        delete U;
                        U  = temp2->fPositions;
                        ux = ix;
                        UinDstates = true;
                        break;
                    }
                }

                // Add U as an unmarked state to Dstates
                if (!UinDstates)
                {
                    RBBIStateDescriptor *newState = new RBBIStateDescriptor(lastInputSymbol, fStatus);
                    if (newState == nullptr) {
                    	*fStatus = U_MEMORY_ALLOCATION_ERROR;
                    }
                    if (U_FAILURE(*fStatus)) {
                        goto ExitBuildSTdeleteall;
                    }
                    newState->fPositions = U;
                    fDStates->addElement(newState, *fStatus);
                    if (U_FAILURE(*fStatus)) {
                        return;
                    }
                    ux = fDStates->size()-1;
                }

                // Dtran[T, a] := U;
                T->fDtran->setElementAt(ux, a);
            }
        }
    }
    return;
    // delete local pointers only if error occurred.
ExitBuildSTdeleteall:
    delete initialState;
    delete failState;
}


/**
 * mapLookAheadRules
 *
 */
void RBBITableBuilder::mapLookAheadRules() {
    fLookAheadRuleMap =  new UVector32(fRB->fScanner->numRules() + 1, *fStatus);
    if (fLookAheadRuleMap == nullptr) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(*fStatus)) {
        return;
    }
    fLookAheadRuleMap->setSize(fRB->fScanner->numRules() + 1);

    for (int32_t n=0; n<fDStates->size(); n++) {
        RBBIStateDescriptor* sd = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(n));
        int32_t laSlotForState = 0;

        // Establish the look-ahead slot for this state, if the state covers
        // any look-ahead nodes - corresponding to the '/' in look-ahead rules.

        // If any of the look-ahead nodes already have a slot assigned, use it,
        // otherwise assign a new one.

        bool sawLookAheadNode = false;
        for (int32_t ipos=0; ipos<sd->fPositions->size(); ++ipos) {
            RBBINode *node = static_cast<RBBINode *>(sd->fPositions->elementAt(ipos));
            if (node->fType != RBBINode::NodeType::lookAhead) {
                continue;
            }
            sawLookAheadNode = true;
            int32_t ruleNum = node->fVal;     // Set when rule was originally parsed.
            U_ASSERT(ruleNum < fLookAheadRuleMap->size());
            U_ASSERT(ruleNum > 0);
            int32_t laSlot = fLookAheadRuleMap->elementAti(ruleNum);
            if (laSlot != 0) {
                if (laSlotForState == 0) {
                    laSlotForState = laSlot;
                } else {
                    // TODO: figure out if this can fail, change to setting an error code if so.
                    U_ASSERT(laSlot == laSlotForState);
                }
            }
        }
        if (!sawLookAheadNode) {
            continue;
        }

        if (laSlotForState == 0) {
            laSlotForState = ++fLASlotsInUse;
        }

        // For each look ahead node covered by this state,
        // set the mapping from the node's rule number to the look ahead slot.
        // There can be multiple nodes/rule numbers going to the same la slot.

        for (int32_t ipos=0; ipos<sd->fPositions->size(); ++ipos) {
            RBBINode *node = static_cast<RBBINode *>(sd->fPositions->elementAt(ipos));
            if (node->fType != RBBINode::NodeType::lookAhead) {
                continue;
            }
            int32_t ruleNum = node->fVal;     // Set when rule was originally parsed.
            int32_t existingVal = fLookAheadRuleMap->elementAti(ruleNum);
            (void)existingVal;
            U_ASSERT(existingVal == 0 || existingVal == laSlotForState);
            fLookAheadRuleMap->setElementAt(laSlotForState, ruleNum);
        }
    }

}

//-----------------------------------------------------------------------------
//
//   flagAcceptingStates    Identify accepting states.
//                          First get a list of all of the end marker nodes.
//                          Then, for each state s,
//                              if s contains one of the end marker nodes in its list of tree positions then
//                                  s is an accepting state.
//
//-----------------------------------------------------------------------------
void     RBBITableBuilder::flagAcceptingStates() {
    if (U_FAILURE(*fStatus)) {
        return;
    }
    UVector     endMarkerNodes(*fStatus);
    RBBINode    *endMarker;
    int32_t     i;
    int32_t     n;

    if (U_FAILURE(*fStatus)) {
        return;
    }

    fTree->findNodes(&endMarkerNodes, RBBINode::endMark, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }

    for (i=0; i<endMarkerNodes.size(); i++) {
        endMarker = static_cast<RBBINode*>(endMarkerNodes.elementAt(i));
        for (n=0; n<fDStates->size(); n++) {
            RBBIStateDescriptor* sd = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(n));
            if (sd->fPositions->indexOf(endMarker) >= 0) {
                // Any non-zero value for fAccepting means this is an accepting node.
                // The value is what will be returned to the user as the break status.
                // If no other value was specified, force it to ACCEPTING_UNCONDITIONAL (1).

                if (sd->fAccepting==0) {
                    // State hasn't been marked as accepting yet.  Do it now.
                    sd->fAccepting = fLookAheadRuleMap->elementAti(endMarker->fVal);
                    if (sd->fAccepting == 0) {
                        sd->fAccepting = ACCEPTING_UNCONDITIONAL;
                    }
                }
                if (sd->fAccepting==ACCEPTING_UNCONDITIONAL && endMarker->fVal != 0) {
                    // Both lookahead and non-lookahead accepting for this state.
                    // Favor the look-ahead, because a look-ahead match needs to
                    // immediately stop the run-time engine. First match, not longest.
                    sd->fAccepting = fLookAheadRuleMap->elementAti(endMarker->fVal);
                }
                // implicit else:
                // if sd->fAccepting already had a value other than 0 or 1, leave it be.
            }
        }
    }
}


//-----------------------------------------------------------------------------
//
//    flagLookAheadStates   Very similar to flagAcceptingStates, above.
//
//-----------------------------------------------------------------------------
void     RBBITableBuilder::flagLookAheadStates() {
    if (U_FAILURE(*fStatus)) {
        return;
    }
    UVector     lookAheadNodes(*fStatus);
    RBBINode    *lookAheadNode;
    int32_t     i;
    int32_t     n;

    fTree->findNodes(&lookAheadNodes, RBBINode::lookAhead, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }
    for (i=0; i<lookAheadNodes.size(); i++) {
        lookAheadNode = static_cast<RBBINode*>(lookAheadNodes.elementAt(i));
        U_ASSERT(lookAheadNode->fType == RBBINode::NodeType::lookAhead);

        for (n=0; n<fDStates->size(); n++) {
            RBBIStateDescriptor* sd = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(n));
            int32_t positionsIdx = sd->fPositions->indexOf(lookAheadNode);
            if (positionsIdx >= 0) {
                U_ASSERT(lookAheadNode == sd->fPositions->elementAt(positionsIdx));
                uint32_t lookaheadSlot = fLookAheadRuleMap->elementAti(lookAheadNode->fVal);
                U_ASSERT(sd->fLookAhead == 0 || sd->fLookAhead == lookaheadSlot);
                // if (sd->fLookAhead != 0 && sd->fLookAhead != lookaheadSlot) {
                //     printf("%s:%d Bingo. sd->fLookAhead:%d   lookaheadSlot:%d\n",
                //            __FILE__, __LINE__, sd->fLookAhead, lookaheadSlot);
                // }
                sd->fLookAhead = lookaheadSlot;
            }
        }
    }
}




//-----------------------------------------------------------------------------
//
//    flagTaggedStates
//
//-----------------------------------------------------------------------------
void     RBBITableBuilder::flagTaggedStates() {
    if (U_FAILURE(*fStatus)) {
        return;
    }
    UVector     tagNodes(*fStatus);
    RBBINode    *tagNode;
    int32_t     i;
    int32_t     n;

    if (U_FAILURE(*fStatus)) {
        return;
    }
    fTree->findNodes(&tagNodes, RBBINode::tag, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }
    for (i=0; i<tagNodes.size(); i++) {                   // For each tag node t (all of 'em)
        tagNode = static_cast<RBBINode*>(tagNodes.elementAt(i));

        for (n=0; n<fDStates->size(); n++) {              //    For each state  s (row in the state table)
            RBBIStateDescriptor* sd = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(n));
            if (sd->fPositions->indexOf(tagNode) >= 0) {  //       if  s include the tag node t
                sortedAdd(&sd->fTagVals, tagNode->fVal);
            }
        }
    }
}




//-----------------------------------------------------------------------------
//
//  mergeRuleStatusVals
//
//      Update the global table of rule status {tag} values
//      The rule builder has a global vector of status values that are common
//      for all tables.  Merge the ones from this table into the global set.
//
//-----------------------------------------------------------------------------
void  RBBITableBuilder::mergeRuleStatusVals() {
    //
    //  The basic outline of what happens here is this...
    //
    //    for each state in this state table
    //       if the status tag list for this state is in the global statuses list
    //           record where and
    //           continue with the next state
    //       else
    //           add the tag list for this state to the global list.
    //
    int i;
    int n;

    // Pre-set a single tag of {0} into the table.
    //   We will need this as a default, for rule sets with no explicit tagging.
    if (fRB->fRuleStatusVals->size() == 0) {
        fRB->fRuleStatusVals->addElement(1, *fStatus);  // Num of statuses in group
        fRB->fRuleStatusVals->addElement(static_cast<int32_t>(0), *fStatus); // and our single status of zero
    }

    //    For each state
    for (n=0; n<fDStates->size(); n++) {
        RBBIStateDescriptor* sd = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(n));
        UVector *thisStatesTagValues = sd->fTagVals;
        if (thisStatesTagValues == nullptr) {
            // No tag values are explicitly associated with this state.
            //   Set the default tag value.
            sd->fTagsIdx = 0;
            continue;
        }

        // There are tag(s) associated with this state.
        //   fTagsIdx will be the index into the global tag list for this state's tag values.
        //   Initial value of -1 flags that we haven't got it set yet.
        sd->fTagsIdx = -1;
        int32_t  thisTagGroupStart = 0;   // indexes into the global rule status vals list
        int32_t  nextTagGroupStart = 0;

        // Loop runs once per group of tags in the global list
        while (nextTagGroupStart < fRB->fRuleStatusVals->size()) {
            thisTagGroupStart = nextTagGroupStart;
            nextTagGroupStart += fRB->fRuleStatusVals->elementAti(thisTagGroupStart) + 1;
            if (thisStatesTagValues->size() != fRB->fRuleStatusVals->elementAti(thisTagGroupStart)) {
                // The number of tags for this state is different from
                //    the number of tags in this group from the global list.
                //    Continue with the next group from the global list.
                continue;
            }
            // The lengths match, go ahead and compare the actual tag values
            //    between this state and the group from the global list.
            for (i=0; i<thisStatesTagValues->size(); i++) {
                if (thisStatesTagValues->elementAti(i) !=
                    fRB->fRuleStatusVals->elementAti(thisTagGroupStart + 1 + i) ) {
                    // Mismatch.
                    break;
                }
            }

            if (i == thisStatesTagValues->size()) {
                // We found a set of tag values in the global list that match
                //   those for this state.  Use them.
                sd->fTagsIdx = thisTagGroupStart;
                break;
            }
        }

        if (sd->fTagsIdx == -1) {
            // No suitable entry in the global tag list already.  Add one
            sd->fTagsIdx = fRB->fRuleStatusVals->size();
            fRB->fRuleStatusVals->addElement(thisStatesTagValues->size(), *fStatus);
            for (i=0; i<thisStatesTagValues->size(); i++) {
                fRB->fRuleStatusVals->addElement(thisStatesTagValues->elementAti(i), *fStatus);
            }
        }
    }
}





//-----------------------------------------------------------------------------
//
//    minimizeStates    Minimize the number of states of the DFA, by Algorithm
//                      3.6 in Aho.  The one twist is that instead of starting
//                      from a partition in two groups (accepting and non-
//                      accepting states), we partition according to all
//                      relevant properties of the states, namely the values of:
//                      - fAccepting—which may be non-accepting,
//                        unconditionally accepting, or the index of a
//                        lookahead—;
//                      - fLookAhead;
//                      - fTagsIdx.
//
//-----------------------------------------------------------------------------

void RBBITableBuilder::minimizeStates() {
    if (U_FAILURE(*fStatus)) {
        return;
    }

#if UPRV_HAS_SANITIZER
    if (fDStates->size() > 512) {
        // This algorithm is sluggish on large state machines, and even more sluggish with
        // sanitizers, which leads the fuzzer into the weeds, see
        // https://github.com/unicode-org/icu/pull/3948#issuecomment-4335461360.
        *fStatus = U_REGEX_PATTERN_TOO_BIG;
        return;
    }
#endif
    struct StateType {
        uint32_t fAccepting;
        uint32_t fLookAhead;
        int32_t fTagsIdx;
        bool operator==(StateType const &other) const {
            return fAccepting == other.fAccepting &&
                   fLookAhead == other.fLookAhead &&
                   fTagsIdx == other.fTagsIdx;
        }
    };
    // We wrap `UVector`s in `unique_ptr``s throughout so we can move them,
    // including into `UVector`s (by releasing them from the `unique_ptr`
    // and having the enclosing `UVector` adopt them). 
    // Group the states by types (but we have no maps so this is verbose).
    // If there are no lookaheads and no tags, there are only two types
    // (accepting and non-accepting) in which case this is exactly step 1
    // of Algorithm 3.6.
    struct TypeToStates : UMemory {
        TypeToStates(const StateType &type, UErrorCode &status)
            : type(type), states(prv::make_unique<UVector>(status, status)) {}
        StateType type;
        prv::unique_ptr<UVector> states;
    };
    UVector initialPartition(*fStatus);
    initialPartition.setDeleter(
        [](void *p) { delete static_cast<TypeToStates *>(p); });
    for (int32_t i = 0; i < fDStates->size(); ++i) {
        const RBBIStateDescriptor &state =
            *static_cast<RBBIStateDescriptor *>(fDStates->elementAt(i));
        const StateType type{state.fAccepting,
                             state.fLookAhead,
                             state.fTagsIdx};
        int32_t j = 0;
        for (; j < initialPartition.size(); ++j) {
            auto &[type_j, states] =
                *static_cast<TypeToStates *>(initialPartition.elementAt(j));
            if (type_j == type) {
                states->addElement(i, *fStatus);
                break;
            }
        }
        if (j == initialPartition.size()) {
            auto newEntry = prv::make_unique<TypeToStates>(type, *fStatus, *fStatus);
            if (U_FAILURE(*fStatus)) {
                return;
            }
            newEntry->states->addElement(i, *fStatus);
            initialPartition.adoptElement(newEntry.release(), *fStatus);
        }
    }
    // The partition Π from Algorithm 3.6.
    // (We could call it Π, but some member companies that integrate the ICU
    // code base prohibit non-ASCII identifiers…).
    auto partition = prv::make_unique<UVector>(*fStatus, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }
    partition->setDeleter(
        [](void *p) { delete static_cast<UVector*>(p); });
    
    for (int32_t i = 0; i < initialPartition.size(); ++i) {
        partition->adoptElement(
            static_cast<TypeToStates*>(initialPartition.elementAt(i))->states.release(),
            *fStatus);
    }
    // Given the index of a state 𝑠 in `fDStates`, returns a UVector of integers
    // σ(𝑠) such that 𝑠.fDtran[i] (the index of the state reached by the
    // transition from 𝑠 on the character class with index i) is in Π[σ(𝑠)[i]].
    // We then have σ(𝑠) == σ(𝑡) if and only if “for all input symbols 𝑎, states
    // 𝑠 and 𝑡 have transitions on 𝑎 to states in the same group of Π” (which is
    // what defines the refinement of 𝐺 in Fig. 3.45).
    auto partition_signature = [&partition, this](int32_t stateIndex) -> prv::unique_ptr<UVector> {
        const RBBIStateDescriptor &state =
            *static_cast<RBBIStateDescriptor *>(fDStates->elementAt(stateIndex));
        auto result = prv::make_unique<UVector>(state.fDtran->size(), *fStatus, *fStatus);
        if (U_FAILURE(*fStatus)) {
            return nullptr;
        }
        for (int32_t i = 0; i < state.fDtran->size(); ++i) {
            int32_t toState = state.fDtran->elementAti(i);
            for (int32_t partIndex = 0; partIndex < partition->size(); ++partIndex) {
                const UVector &part = *static_cast<UVector *>(partition->elementAt(partIndex));
                if (part.contains(toState)) {
                    result->addElement(partIndex, *fStatus);
                    break;
                }
            }
        }
        result->setComparer([](const UElement left, const UElement right) -> UBool {
            return left.integer == right.integer;
        });
        return result;
    };
    // The loop between steps 2. and 3. of Algorithm 3.6.
    for (;;) {
        // Π_new.
        auto partitionNew = prv::make_unique<UVector>(*fStatus, *fStatus);
        if (U_FAILURE(*fStatus)) {
            return;
        }
        partitionNew->setDeleter(
            [](void *p) { delete static_cast<UVector*>(p); });
        // The procedure from Figure 3.45, Construction of Π_new.
        bool refined = false;
        for (int32_t i = 0; i < partition->size(); ++i) {
            // Group 𝐺 in Π (=`partition`).
            const UVector &group = *static_cast<UVector*>(partition->elementAt(i));
            // Partition 𝐺 based on the signature, see above.
            struct SignatureToStates : UMemory {
                SignatureToStates(prv::unique_ptr<UVector> signature, UErrorCode &status)
                    : signature(std::move(signature)), states(prv::make_unique<UVector>(status, status)) {
                }
                prv::unique_ptr<UVector> signature;
                prv::unique_ptr<UVector> states;
            };
            UVector groupRefinement(*fStatus);
            groupRefinement.setDeleter([](void *p) { delete static_cast<SignatureToStates*>(p); });
            for (int32_t j = 0; j < group.size(); ++j) {
                // Index of a state in the group.
                const int32_t s = group.elementAti(j);
                auto signature = partition_signature(s);
                int32_t k = 0;
                for (; k < groupRefinement.size(); ++k) {
                    const auto &[subgroupSignature, subgroup] =
                        *static_cast<SignatureToStates*>(groupRefinement.elementAt(k));
                    if (*subgroupSignature == *signature) {
                        subgroup->addElement(s, *fStatus);
                        break;
                    }
                }
                if (k == groupRefinement.size()) {
                    const auto newEntry =
                        new SignatureToStates(std::move(signature), *fStatus);
                    if (U_FAILURE(*fStatus)) {
                        return;
                    }
                    if (newEntry == nullptr) {
                      *fStatus = U_MEMORY_ALLOCATION_ERROR;
                      return;
                    }
                    newEntry->states->addElement(s, *fStatus);
                    groupRefinement.adoptElement(newEntry, *fStatus);
                }
            }
            refined |= groupRefinement.size() > 1;
            for (int32_t j = 0; j < groupRefinement.size(); ++j) {
                auto &[_, subgroup] = *static_cast<SignatureToStates*>(
                    groupRefinement.elementAt(j));
                partitionNew->adoptElement(subgroup.release(), *fStatus);
            }
        }
        if (refined) {
            partition = std::move(partitionNew);
        } else {
            break;
        }
    }
    // We will use the indices in `partition` as the new state indices.  Make
    // sure that the start (1) and stop (0) states remain in their correct
    // places; everything else can merrily be scrambled.
    for (int i = 0; i < partition->size(); ++i) {
        UVector *const part_i = static_cast<UVector *>(partition->elementAt(i));
        if (part_i->contains(0)) {
            void *const part_0 = partition->elementAt(0);
            // No swap on UVector, so we briefly remove the deleter.
            const auto deleter = partition->setDeleter(nullptr);
            partition->setElementAt(part_i, 0);
            partition->setElementAt(part_0, i);
            partition->setDeleter(deleter);
        } else if (part_i->contains(1)) {
            void *const part_1 = partition->elementAt(1);
            const auto deleter = partition->setDeleter(nullptr);
            partition->setElementAt(part_i, 1);
            partition->setElementAt(part_1, i);
            partition->setDeleter(deleter);
        }
    }
    if (U_FAILURE(*fStatus)) {
        return;
    }
    auto oldStateToPart = prv::make_unique_for_overwrite<uint16_t[]>(fDStates->size(), *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }
    for (int i = 0; i < partition->size(); ++i) {
        const UVector &part = *static_cast<UVector*>(partition->elementAt(i));
        for (int j = 0; j < part.size(); ++j) {
            oldStateToPart[part.elementAti(j)] = i;
        }
    }
    auto const oldStates = prv::unique_ptr<UVector>(fDStates);
    fDStates = prv::make_unique<UVector>(*fStatus, *fStatus).release();
    if (U_FAILURE(*fStatus)) {
        return;
    }
    for (int i = 0; i < partition->size(); ++i) {
        const UVector &part = *static_cast<UVector*>(partition->elementAt(i));
        RBBIStateDescriptor *const state =
            static_cast<RBBIStateDescriptor*>(oldStates->elementAt(part.elementAti(0)));
        fDStates->addElement(state, *fStatus);
        for (int j = 0; j < state->fDtran->size(); ++j) {
            state->fDtran->setElementAt(oldStateToPart[state->fDtran->elementAti(j)], j);
        }
        oldStates->setElementAt(nullptr, part.elementAti(0));
    }
    oldStates->setDeleter([](void *p) { delete static_cast<RBBIStateDescriptor*>(p); });
}







//-----------------------------------------------------------------------------
//
//  sortedAdd  Add a value to a vector of sorted values (ints).
//             Do not replicate entries; if the value is already there, do not
//                add a second one.
//             Lazily create the vector if it does not already exist.
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::sortedAdd(UVector **vector, int32_t val) {
    int32_t i;

    if (*vector == nullptr) {
        *vector = new UVector(*fStatus);
    }
    if (*vector == nullptr || U_FAILURE(*fStatus)) {
        return;
    }
    UVector *vec = *vector;
    int32_t  vSize = vec->size();
    for (i=0; i<vSize; i++) {
        int32_t valAtI = vec->elementAti(i);
        if (valAtI == val) {
            // The value is already in the vector.  Don't add it again.
            return;
        }
        if (valAtI > val) {
            break;
        }
    }
    vec->insertElementAt(val, i, *fStatus);
}



//-----------------------------------------------------------------------------
//
//  setAdd     Set operation on UVector
//             dest = dest union source
//             Elements may only appear once and must be sorted.
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::setAdd(UVector *dest, UVector *source) {
    U_ASSERT(!dest->hasDeleter());
    U_ASSERT(!source->hasDeleter());
    int32_t destOriginalSize = dest->size();
    int32_t sourceSize       = source->size();
    int32_t di           = 0;
    MaybeStackArray<void *, 16> destArray, sourceArray;  // Handle small cases without malloc
    void **destPtr, **sourcePtr;
    void **destLim, **sourceLim;

    if (destOriginalSize > destArray.getCapacity()) {
        if (destArray.resize(destOriginalSize) == nullptr) {
            return;
        }
    }
    destPtr = destArray.getAlias();
    destLim = destPtr + destOriginalSize;  // destArray.getArrayLimit()?

    if (sourceSize > sourceArray.getCapacity()) {
        if (sourceArray.resize(sourceSize) == nullptr) {
            return;
        }
    }
    sourcePtr = sourceArray.getAlias();
    sourceLim = sourcePtr + sourceSize;  // sourceArray.getArrayLimit()?

    // Avoid multiple "get element" calls by getting the contents into arrays
    (void) dest->toArray(destPtr);
    (void) source->toArray(sourcePtr);

    dest->setSize(sourceSize+destOriginalSize, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }

    while (sourcePtr < sourceLim && destPtr < destLim) {
        if (*destPtr == *sourcePtr) {
            dest->setElementAt(*sourcePtr++, di++);
            destPtr++;
        }
        // This check is required for machines with segmented memory, like i5/OS.
        // Direct pointer comparison is not recommended.
        else if (uprv_memcmp(destPtr, sourcePtr, sizeof(void *)) < 0) {
            dest->setElementAt(*destPtr++, di++);
        }
        else { /* *sourcePtr < *destPtr */
            dest->setElementAt(*sourcePtr++, di++);
        }
    }

    // At most one of these two cleanup loops will execute
    while (destPtr < destLim) {
        dest->setElementAt(*destPtr++, di++);
    }
    while (sourcePtr < sourceLim) {
        dest->setElementAt(*sourcePtr++, di++);
    }

    dest->setSize(di, *fStatus);
}



//-----------------------------------------------------------------------------
//
//  setEqual    Set operation on UVector.
//              Compare for equality.
//              Elements must be sorted.
//
//-----------------------------------------------------------------------------
UBool RBBITableBuilder::setEquals(UVector *a, UVector *b) {
    return a->equals(*b);
}


//-----------------------------------------------------------------------------
//
//  printPosSets   Debug function.  Dump Nullable, firstpos, lastpos and followpos
//                 for each node in the tree.
//
//-----------------------------------------------------------------------------
#ifdef RBBI_DEBUG
void RBBITableBuilder::printPosSets(RBBINode *n) {
    if (n==nullptr) {
        return;
    }
    printf("\n");
    RBBINode::printNodeHeader();
    RBBINode::printNode(n);
    RBBIDebugPrintf("         Nullable:  %s\n", n->fNullable?"true":"false");

    RBBIDebugPrintf("         firstpos:  ");
    printSet(n->fFirstPosSet);

    RBBIDebugPrintf("         lastpos:   ");
    printSet(n->fLastPosSet);

    RBBIDebugPrintf("         followpos: ");
    printSet(n->fFollowPos);

    printPosSets(n->fLeftChild);
    printPosSets(n->fRightChild);
}
#endif

//
//    findDuplCharClassFrom()
//
bool RBBITableBuilder::findDuplCharClassFrom(IntPair *categories) {
    int32_t numStates = fDStates->size();
    int32_t numCols = fRB->fSetBuilder->getNumCharCategories();

    for (; categories->first < numCols-1; categories->first++) {
        // Note: dictionary & non-dictionary columns cannot be merged.
        //       The limitSecond value prevents considering mixed pairs.
        //       Dictionary categories are >= DictCategoriesStart.
        //       Non dict categories are   <  DictCategoriesStart.
        int limitSecond = categories->first < fRB->fSetBuilder->getDictCategoriesStart() ?
            fRB->fSetBuilder->getDictCategoriesStart() : numCols;
        for (categories->second=categories->first+1; categories->second < limitSecond; categories->second++) {
            // Initialized to different values to prevent returning true if numStates = 0 (implies no duplicates).
            uint16_t table_base = 0;
            uint16_t table_dupl = 1;
            for (int32_t state=0; state<numStates; state++) {
                RBBIStateDescriptor* sd = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(state));
                table_base = static_cast<uint16_t>(sd->fDtran->elementAti(categories->first));
                table_dupl = static_cast<uint16_t>(sd->fDtran->elementAti(categories->second));
                if (table_base != table_dupl) {
                    break;
                }
            }
            if (table_base == table_dupl) {
                return true;
            }
        }
    }
    return false;
}


//
//    removeColumn()
//
void RBBITableBuilder::removeColumn(int32_t column) {
    int32_t numStates = fDStates->size();
    for (int32_t state=0; state<numStates; state++) {
        RBBIStateDescriptor* sd = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(state));
        U_ASSERT(column < sd->fDtran->size());
        sd->fDtran->removeElementAt(column);
    }
}


bool RBBITableBuilder::findDuplicateSafeState(IntPair *states) {
    int32_t numStates = fSafeTable->size();

    for (; states->first<numStates-1; states->first++) {
        UnicodeString *firstRow = static_cast<UnicodeString *>(fSafeTable->elementAt(states->first));
        for (states->second=states->first+1; states->second<numStates; states->second++) {
            UnicodeString *duplRow = static_cast<UnicodeString *>(fSafeTable->elementAt(states->second));
            bool rowsMatch = true;
            int32_t numCols = firstRow->length();
            for (int32_t col=0; col < numCols; ++col) {
                int32_t firstVal = firstRow->charAt(col);
                int32_t duplVal = duplRow->charAt(col);
                if (!((firstVal == duplVal) ||
                        ((firstVal == states->first || firstVal == states->second) &&
                        (duplVal  == states->first || duplVal  == states->second)))) {
                    rowsMatch = false;
                    break;
                }
            }
            if (rowsMatch) {
                return true;
            }
        }
    }
    return false;
}

void RBBITableBuilder::removeSafeState(IntPair duplStates) {
    const int32_t keepState = duplStates.first;
    const int32_t duplState = duplStates.second;
    U_ASSERT(keepState < duplState);
    U_ASSERT(duplState < fSafeTable->size());

    fSafeTable->removeElementAt(duplState);   // Note that fSafeTable has a deleter function
                                              // and will auto-delete the removed element.
    int32_t numStates = fSafeTable->size();
    for (int32_t state=0; state<numStates; ++state) {
        UnicodeString* sd = static_cast<UnicodeString*>(fSafeTable->elementAt(state));
        int32_t numCols = sd->length();
        for (int32_t col=0; col<numCols; col++) {
            int32_t existingVal = sd->charAt(col);
            int32_t newVal = existingVal;
            if (existingVal == duplState) {
                newVal = keepState;
            } else if (existingVal > duplState) {
                newVal = existingVal - 1;
            }
            sd->setCharAt(col, static_cast<char16_t>(newVal));
        }
    }
}


/*
 * RemoveDuplicateStates
 */
int32_t RBBITableBuilder::removeDuplicateStates() {
    const int32_t oldStateCount = fDStates->size();
    minimizeStates();
    return fDStates->size() - oldStateCount;
}


/*
 * Minimizes the number of slots used by the lookaheads.
 *
 * When the state machine is first generated, every lookahead rule occupies a different slot, for instance,
 * given
 *   x / z;  # Lookahead 1 below.
 *   y / z;  # Lookahead 2 below.
 *   [xyz]*;
 * the following state machine is produced,
 *           x
 *         x ⮏  z
 *         → 𝑠₁ → 𝑎₁
 *   START  x⇅y
 *     ↻z  → 𝑠₂ → 𝑎₂
 *         y ↻  z
 *           y
 *
 * where 𝑠ᵢ sets the current position for lookahead i (assigning pᵢ=p_current), and 𝑎ᵢ accepts
 * lookahead i if set, i.e., on state 𝑎ᵢ,the break iterator returns pᵢ if set. Because they accept
 * different lookaheads, states 𝑎₁ and 𝑎₂ are distinct in the initial partition constructed by
 * `minimizeStates`, and cannot be merged, and the state machine cannot be simplified.
 *
 * However, there is no need for lookaheads 1 and 2 to occupy different slots, i.e., there is no
 * need for the state machine to separately store the positions set by 𝑠₁ and 𝑠₂ (for p₁ and p₂ to
 * be distinct variables). Indeed, if state 𝑠₁ is encountered, state 𝑎₂ cannot be reached without
 * going through state 𝑠₂, and vice versa, so if p₁ and p₂ are backed by the same variable p, the
 * value set by p=p₁=p_current on 𝑠₁ will have been overriden by p=p₂=p_current on 𝑠₂, so that the
 * p₁-p₂ merger has no effect.
 *
 * If the lookaheads are merged, states 𝑎₁ and 𝑎₂ are no longer initially distinct in
 * `minimizeStates` (they both accept the only lookahead), and indeed the whole state machine
 * simplifies to
 *         [xy]     z
 *   START   →  𝑠  →  𝑎
 *     ↻z       ↻
 *             [xy]
 *
 * Lookaheads i and j can be merged if:
 * 1. From any state that sets lookahead i, no state that accepts lookahead j can be reached without
 *    going through a state that sets lookahead j; and
 * 2. From any state that sets lookahead j, no state that accepts lookahead i can be reached without
 *    going through a state that sets lookahead i.
 * This reachability relation defines a directed graph of lookaheads, and an optimal merging of
 * lookaheads is then a colouring of that graph. (In the example above, the graph of lookaheads has
 * no edges, and is thus 1-colourable; more interesting examples can be found in the test
 * TestLookaheadPolychromy.)
 *
 * This function first computes the adjacency matrix of lookahead reachability, and then colours the
 * graph of lookaheads and reassigns lookahead slots accordingly.
 */
void RBBITableBuilder::minimizeLookaheads() {
    if (fLASlotsInUse == ACCEPTING_UNCONDITIONAL) {
        return;
    }
    const int32_t lookaheadCount = fLASlotsInUse - ACCEPTING_UNCONDITIONAL;
    // The last lookahead is fLASlotsInUse.
    const int32_t firstLookahead = ACCEPTING_UNCONDITIONAL + 1;
    const auto stateDescriptor = [this](const int32_t state) -> RBBIStateDescriptor & {
        return *static_cast<RBBIStateDescriptor *>(fDStates->elementAt(state));
    };

    const auto lookaheadReachability =
        prv::make_unique<bool[]>(lookaheadCount * lookaheadCount, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }
    for (int32_t l = firstLookahead; l <= fLASlotsInUse; ++l) {
        for (int32_t k = firstLookahead; k <= fLASlotsInUse; ++k) {
            if (k != l) {
                for (int32_t source = 1; source < fDStates->size(); ++source) {
                    if (static_cast<int32_t>(stateDescriptor(source).fLookAhead) != l) {
                        continue;
                    }
                    if (reachableByTransitions(
                            *fDStates, source, /*isSink=*/
                            [&](const int32_t state) {
                                return static_cast<int32_t>(stateDescriptor(state).fAccepting) == k;
                            },
                            /*excludedState=*/
                            [&](const int32_t state) {
                                return static_cast<int32_t>(stateDescriptor(state).fLookAhead) == k;
                            },
                            *fStatus)) {
                        lookaheadReachability[(l - firstLookahead) * lookaheadCount + k -
                                              firstLookahead] = true;
                    }
                }
            }
        }
    }

    const auto colours = prv::make_unique_for_overwrite<int32_t[]>(lookaheadCount, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }
    // Brute-force colouring: the number of lookaheads is in practice small, and the chromatic
    // number is in practice 1, so this usually terminates in one iteration of the first two loops.
    // Of course the worst case is exponential, but exponentials are everywhere in regular
    // expressions anyway.
    // The outer loop terminates after at most `lookaheadCount` iterations
    // (when the chromatic number equals the number of lookaheads being coloured).
    for (int32_t chromaticNumber = 1;; ++chromaticNumber) {
        uprv_memset(colours.get(), 0, sizeof(int32_t) * lookaheadCount);
        // This loop terminates after at most chromaticNumber ** lookaheadCount iterations.
        for (;;) {
            // Here `source` and `sink` correspond to lookaheads, not states.
            for (int source = 0; source < lookaheadCount; ++source) {
                for (int sink = 0; sink < lookaheadCount; ++sink) {
                    if (lookaheadReachability[source * lookaheadCount + sink] &&
                        colours[source] == colours[sink]) {
                        goto nextColouring;
                    }
                }
            }

            // We have found a valid colouring of the graph of lookaheads.  Assign lookahead slots
            // accordingly.
            for (int i = 0; i < fDStates->size(); ++i) {
                if (stateDescriptor(i).fAccepting > ACCEPTING_UNCONDITIONAL) {
                    stateDescriptor(i).fAccepting =
                        firstLookahead +
                        colours[stateDescriptor(i).fAccepting - firstLookahead];
                }
                if (stateDescriptor(i).fLookAhead != 0) {
                    stateDescriptor(i).fLookAhead =
                        ACCEPTING_UNCONDITIONAL + 1 +
                        colours[stateDescriptor(i).fLookAhead - firstLookahead];
                }
            }
            fLASlotsInUse = ACCEPTING_UNCONDITIONAL + chromaticNumber;
            return;

          nextColouring:
            // Increment the colour of the first lookahead, and then carry if we reach
            // `chromaticNumber` (which is one more than the greatest colour).
            ++colours[0];
            for (int32_t i = 0; i < lookaheadCount - 1 && colours[i] == chromaticNumber;
                 ++i) {
                colours[i] = 0;
                ++colours[i + 1];
            }
            if (colours[lookaheadCount - 1] == chromaticNumber) {
                // We tried all assignments of colours in {0, …, chromaticNumber - 1},
                // we need more colours.
                break;
            }
        }
    }
}


//-----------------------------------------------------------------------------
//
//   getTableSize()    Calculate the size of the runtime form of this
//                     state transition table.
//
//-----------------------------------------------------------------------------
int32_t  RBBITableBuilder::getTableSize() const {
    int32_t    size = 0;
    int32_t    numRows;
    int32_t    numCols;
    int32_t    rowSize;

    if (fTree == nullptr) {
        return 0;
    }

    size    = offsetof(RBBIStateTable, fTableData);    // The header, with no rows to the table.

    numRows = fDStates->size();
    numCols = fRB->fSetBuilder->getNumCharCategories();

    if (use8BitsForTable()) {
        rowSize = offsetof(RBBIStateTableRow8, fNextState) + sizeof(int8_t)*numCols;
    } else {
        rowSize = offsetof(RBBIStateTableRow16, fNextState) + sizeof(int16_t)*numCols;
    }
    size   += numRows * rowSize;
    return size;
}

bool RBBITableBuilder::use8BitsForTable() const {
    return fDStates->size() <= kMaxStateFor8BitsTable;
}

//-----------------------------------------------------------------------------
//
//   exportTable()    export the state transition table in the format required
//                    by the runtime engine.  getTableSize() bytes of memory
//                    must be available at the output address "where".
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::exportTable(void *where) {
    RBBIStateTable* table = static_cast<RBBIStateTable*>(where);
    uint32_t           state;
    int                col;

    if (U_FAILURE(*fStatus) || fTree == nullptr) {
        return;
    }

    int32_t catCount = fRB->fSetBuilder->getNumCharCategories();
    if (catCount > 0x7fff ||
        fDStates->size() > 0x7fff) {
        *fStatus = U_BRK_INTERNAL_ERROR;
        return;
    }

    table->fNumStates = fDStates->size();
    table->fDictCategoriesStart = fRB->fSetBuilder->getDictCategoriesStart();
    table->fLookAheadResultsSize = fLASlotsInUse == ACCEPTING_UNCONDITIONAL ? 0 : fLASlotsInUse + 1;
    table->fFlags     = 0;
    if (use8BitsForTable()) {
        table->fRowLen    = offsetof(RBBIStateTableRow8, fNextState) + sizeof(uint8_t) * catCount;
        table->fFlags  |= RBBI_8BITS_ROWS;
    } else {
        table->fRowLen    = offsetof(RBBIStateTableRow16, fNextState) + sizeof(int16_t) * catCount;
    }
    if (fRB->fLookAheadHardBreak) {
        table->fFlags  |= RBBI_LOOKAHEAD_HARD_BREAK;
    }

    for (state=0; state<table->fNumStates; state++) {
        RBBIStateDescriptor* sd = static_cast<RBBIStateDescriptor*>(fDStates->elementAt(state));
        RBBIStateTableRow* row = reinterpret_cast<RBBIStateTableRow*>(table->fTableData + state * table->fRowLen);
        if (use8BitsForTable()) {
            U_ASSERT (sd->fAccepting <= 255);
            U_ASSERT (sd->fLookAhead <= 255);
            U_ASSERT (0 <= sd->fTagsIdx && sd->fTagsIdx <= 255);
            RBBIStateTableRow8* r8 = reinterpret_cast<RBBIStateTableRow8*>(row);
            r8->fAccepting = sd->fAccepting;
            r8->fLookAhead = sd->fLookAhead;
            r8->fTagsIdx   = sd->fTagsIdx;
            for (col=0; col<catCount; col++) {
                U_ASSERT (sd->fDtran->elementAti(col) <= kMaxStateFor8BitsTable);
                r8->fNextState[col] = sd->fDtran->elementAti(col);
            }
        } else {
            U_ASSERT (sd->fAccepting <= 0xffff);
            U_ASSERT (sd->fLookAhead <= 0xffff);
            U_ASSERT (0 <= sd->fTagsIdx && sd->fTagsIdx <= 0xffff);
            row->r16.fAccepting = sd->fAccepting;
            row->r16.fLookAhead = sd->fLookAhead;
            row->r16.fTagsIdx   = sd->fTagsIdx;
            for (col=0; col<catCount; col++) {
                row->r16.fNextState[col] = sd->fDtran->elementAti(col);
            }
        }
    }
}


/**
 *   Synthesize a safe state table from the main state table.
 */
void RBBITableBuilder::buildSafeReverseTable(UErrorCode &status) {
    // The safe table creation has three steps:

    // 1. Identify pairs of character classes that are "safe." Safe means that boundaries
    // following the pair do not depend on context or state before the pair. To test
    // whether a pair is safe, run it through the main forward state table, starting
    // from each possible configuration. If the final configuration is the same, no matter what the
    // starting configuration, the pair is safe.
    // Note that the configuration includes not only the state, but also the last accepting position
    // and the lookaheads.  Some differences in final lookaheads can be ignored when it can be shown
    // that they do not affect breaks after the pair: this happens when we come back out of the pair
    // with a matching configuration when the lookahead matches.
    // TODO(egg): It is possible for pairs to be safe even if the outgoing states differ depending
    // on the state before the pair; this would require a more thorough analysis to handle, probably
    // in the style of what happens in minimizeStates.
    //
    // 2. Build a state table that recognizes the safe pairs. It's similar to their
    // forward table, with a column for each input character [class], and a row for
    // each state. Row 1 is the start state, and row 0 is the stop state. Initially
    // create an additional state for each input character category; being in
    // one of these states means that the character has been seen, and is potentially
    // the first of a pair. In each of these rows, the entry for the second character
    // of a safe pair is set to the stop state (0), indicating that a match was found.
    // All other table entries are set to the state corresponding the current input
    // character, allowing that character to be the of a start following pair.
    //
    // Because the safe rules are to be run in reverse, moving backwards in the text,
    // the first and second pair categories are swapped when building the table.
    //
    // 3. Compress the table. There are typically many rows (states) that are
    // equivalent - that have zeroes (match completed) in the same columns -
    // and can be folded together.

    // Each safe pair is stored as two UChars in the safePair string.
    UnicodeString safePairs;

    int32_t numCharClasses = fRB->fSetBuilder->getNumCharCategories();
    int32_t numStates = fDStates->size();
    // The elements at indices 0 and ACCEPTING_UNCONDITIONAL (1) are never used.
    const auto lookaheadPositions =
        prv::make_unique_for_overwrite<int8_t[]>(fLASlotsInUse + 1, status);
    const auto wantedLookaheadsInPair =
        prv::make_unique_for_overwrite<int8_t[]>(fLASlotsInUse + 1, status);
    if (U_FAILURE(status)) {
        return;
    }
    // Same as in rbbi.cpp.
    // TODO(egg): Maybe this should move to rbbidata.h like ACCEPTING_UNCONDITIONAL?
    constexpr int32_t START_STATE = 1;
    // Index -2 is used to signify a text position before the pair (c1, c2).
    constexpr int32_t BEFORE_PAIR = -2;
    // In lookaheadPositions, index -1 is used to signify that the lookahead has not been set.
    constexpr int8_t LOOKAHEAD_NOT_SET = -1;
    for (int32_t c1 = 1; c1 < numCharClasses; ++c1) {
        if (fRB->fSetBuilder->getFirstChar(c1) < 0) {
          continue;
        }
        for (int32_t c2 = 1; c2 < numCharClasses; ++c2) {
            if (fRB->fSetBuilder->getFirstChar(c2) < 0) {
                continue;
            }
            int32_t wantedEndState = -1;
            int32_t wantedLastAcceptingPosition;
            bool mustCheckConsistentWithStartC2 = false;
            const int32_t text[2] = {c1, c2};
            for (int32_t injectedState = START_STATE; injectedState < numStates; ++injectedState) {
                for (int32_t injectedLookahead = ACCEPTING_UNCONDITIONAL;
                     injectedLookahead <= fLASlotsInUse; ++injectedLookahead) {
                    int32_t lastBreak = BEFORE_PAIR;
                    int32_t lastAcceptingPosition = BEFORE_PAIR;
                    uprv_memset(lookaheadPositions.get(), LOOKAHEAD_NOT_SET, fLASlotsInUse + 1);
                    if (injectedLookahead != ACCEPTING_UNCONDITIONAL) {
                        lookaheadPositions[injectedLookahead] = BEFORE_PAIR;
                    }
                    {
                        const RBBIStateDescriptor &injectedStateDescriptor =
                            *static_cast<RBBIStateDescriptor *>(fDStates->elementAt(injectedState));
                        if (injectedStateDescriptor.fAccepting == ACCEPTING_UNCONDITIONAL) {
                            lastAcceptingPosition = 0;
                        } else if (static_cast<int32_t>(injectedStateDescriptor.fAccepting) ==
                                   injectedLookahead) {
                            continue;
                        }
                        if (injectedStateDescriptor.fLookAhead != 0) {
                            lookaheadPositions[injectedStateDescriptor.fLookAhead] = 0;
                        }
                    }
                    int32_t s = injectedState;
                    int32_t i = 0;
                    goto dfaStep;
                    // Loop Run_DFA in L2/26-135, without the termination condition (we do not need
                    // to simulate the end of text).
                    for (;;) {
                        s = START_STATE;
                        uprv_memset(lookaheadPositions.get(), LOOKAHEAD_NOT_SET, fLASlotsInUse + 1);
                        i = lastBreak;
                        // Loop Find_Next_Break in L2/26-135.
                        for (;;) {
                        dfaStep:
                            if (i == BEFORE_PAIR || i == 2) {
                                goto inspectFinalState;
                            }
                            const int32_t symbolAhead = text[i];
                            ++i;
                            const RBBIStateDescriptor *sDescriptor =
                                static_cast<RBBIStateDescriptor *>(fDStates->elementAt(s));
                            if (sDescriptor->fDtran->elementAti(symbolAhead) != 0) {
                                s = sDescriptor->fDtran->elementAti(symbolAhead);
                                sDescriptor =
                                    static_cast<RBBIStateDescriptor *>(fDStates->elementAt(s));
                            } else {
                                if (lastBreak != BEFORE_PAIR &&
                                    lastAcceptingPosition == lastBreak) {
                                    // The pair (c1, c2) puts us in an infinite loop from the
                                    // current configuration, presumably because some one-character
                                    // strings are not in the language recognized by the rules).
                                    // This should never happen for a segmentation algorithm.  The
                                    // RBBI implementation forcefully advances the iterator in that
                                    // case; let’s just say the pair is unsafe and move on.
                                    goto nextSymbolPair;
                                }
                                lastBreak = lastAcceptingPosition;
                                break;
                            }

                            if (sDescriptor->fAccepting == ACCEPTING_UNCONDITIONAL) {
                                lastAcceptingPosition = i;
                            } else if (sDescriptor->fAccepting > ACCEPTING_UNCONDITIONAL &&
                                       lookaheadPositions[sDescriptor->fAccepting] !=
                                           LOOKAHEAD_NOT_SET) {
                                lastBreak = lookaheadPositions[sDescriptor->fAccepting];
                                break;
                            }
                            if (sDescriptor->fLookAhead != 0) {
                                lookaheadPositions[sDescriptor->fLookAhead] = i;
                            }
                        }
                    }
                inspectFinalState:
                    if (i == BEFORE_PAIR) {
                        // Starting on `injectedState` with `injectedLookahead` set, with the pair
                        // (c1, c2) ahead, the state machine finds a break before the pair; it will
                        // thus come back to the pair in a different configuration, which is covered
                        // by some other iteration of the loop over `injectedState` and
                        // `injectedLookahead`.
                        continue;
                    }
                    if (wantedEndState < 0) {
                        wantedEndState = s;
                        wantedLastAcceptingPosition = lastAcceptingPosition;
                        for (int32_t l = ACCEPTING_UNCONDITIONAL + 1; l <= fLASlotsInUse; ++l) {
                            if (lookaheadPositions[l] != LOOKAHEAD_NOT_SET &&
                                lookaheadPositions[l] != BEFORE_PAIR &&
                                lookaheadPositions[l] != 0) {
                                wantedLookaheadsInPair[l] = lookaheadPositions[i];
                            } else {
                                wantedLookaheadsInPair[l] = LOOKAHEAD_NOT_SET;
                            }
                        }
                    } else {
                        if (wantedEndState != s) {
                            // We can get out of the pair on different states depending on the
                            // initial configuration.
                            goto nextSymbolPair;
                        }
                        if (wantedLastAcceptingPosition != lastAcceptingPosition) {
                            goto nextSymbolPair;
                        }
                    }
                    // A lookahead some distance before the pair (set to BEFORE_PAIR) means we
                    // will come back to the pair in a different configuration if it matches, a
                    // lookahead at position 0 means we get back to the pair on the start state
                    // if it matches; either way, this is covered by other iterations of the
                    // loop over injected states and lookaheads.
                    // For lookaheads at position 1 (in the middle of the pair) and 2 (after the
                    // pair), these have two ways of being safe: either they are always set, or it
                    // does not matter if they are set, because if they match we come back with the
                    // same state.
                    for (int32_t l = ACCEPTING_UNCONDITIONAL + 1; l <= fLASlotsInUse; ++l) {
                        if (lookaheadPositions[l] == BEFORE_PAIR || lookaheadPositions[l] == 0 ||
                            lookaheadPositions[l] == wantedLookaheadsInPair[l]) {
                            continue;
                        }
                        if (lookaheadPositions[l] == 2 || wantedLookaheadsInPair[l] == 2) {
                            // A lookahead after the pair means we will come back to the end of the
                            // pair on the start state if it matches, so in order for the pair to be
                            // safe if that lookahead is not consistently set, we must have left it
                            // on the start state.
                            if (wantedEndState != START_STATE) {
                                goto nextSymbolPair;
                            }
                        }
                        if (lookaheadPositions[l] == 1 || wantedLookaheadsInPair[l] == 1) {
                            // If a lookahead in the middle of the pair matches, we will come back
                            // to c2 on the start state.
                            mustCheckConsistentWithStartC2 = true;
                        }
                    }
                }
            }
            if (mustCheckConsistentWithStartC2) {
                const RBBIStateDescriptor& startDescriptor =
                    *static_cast<RBBIStateDescriptor *>(fDStates->elementAt(START_STATE));
                const auto c2State = startDescriptor.fDtran->elementAti(c2);
                if (c2State != wantedEndState) {
                    goto nextSymbolPair;
                }
                const RBBIStateDescriptor& c2StateDescriptor =
                    *static_cast<RBBIStateDescriptor *>(fDStates->elementAt(c2State));
                // In a segmentation algorithm, any single symbol transitions to an
                // accepting state from the start state (a single character is a
                // possible segment), so we know c2State is accepting.
                U_ASSERT(c2StateDescriptor.fAccepting == ACCEPTING_UNCONDITIONAL);
                if (wantedLastAcceptingPosition != 2) {
                    goto nextSymbolPair;
                }
                // If that in turn sets a lookahead, we have a lookahead after the
                // pair.  If we consistently have that the pair is safe.  Otherwise, if we match
                // that lookahead, we end up on the start state.
                for (int32_t l = ACCEPTING_UNCONDITIONAL + 1; l <= fLASlotsInUse; ++l) {
                    if (((wantedLookaheadsInPair[l] == 2) !=
                         (static_cast<int32_t>(c2StateDescriptor.fLookAhead) == l)) &&
                        wantedEndState != START_STATE) {
                        goto nextSymbolPair;
                    }
                }
            }
            safePairs.append(static_cast<char16_t>(c1));
            safePairs.append(static_cast<char16_t>(c2));
        nextSymbolPair:
            continue;
        }
    }

    // Populate the initial safe table.
    // The table as a whole is UVector<UnicodeString>
    // Each row is represented by a UnicodeString, being used as a Vector<int16>.
    // Row 0 is the stop state.
    // Row 1 is the start state.
    // Row 2 and beyond are other states, initially one per char class, but
    //   after initial construction, many of the states will be combined, compacting the table.
    // The String holds the nextState data only. The four leading fields of a row, fAccepting,
    // fLookAhead, etc. are not needed for the safe table, and are omitted at this stage of building.

    U_ASSERT(fSafeTable == nullptr);
    LocalPointer<UVector> lpSafeTable(
        new UVector(uprv_deleteUObject, uhash_compareUnicodeString, numCharClasses + 2, status), status);
    if (U_FAILURE(status)) {
        return;
    }
    fSafeTable = lpSafeTable.orphan();
    for (int32_t row=0; row<numCharClasses + 2; ++row) {
        LocalPointer<UnicodeString> lpString(new UnicodeString(numCharClasses, 0, numCharClasses+4), status);
        fSafeTable->adoptElement(lpString.orphan(), status);
    }
    if (U_FAILURE(status)) {
        return;
    }

    // From the start state, each input char class transitions to the state for that input.
    UnicodeString &startState = *static_cast<UnicodeString *>(fSafeTable->elementAt(1));
    for (int32_t charClass=0; charClass < numCharClasses; ++charClass) {
        // Note: +2 for the start & stop state.
        startState.setCharAt(charClass, static_cast<char16_t>(charClass+2));
    }

    // Initially make every other state table row look like the start state row,
    for (int32_t row=2; row<numCharClasses+2; ++row) {
        UnicodeString &rowState = *static_cast<UnicodeString *>(fSafeTable->elementAt(row));
        rowState = startState;   // UnicodeString assignment, copies contents.
    }

    // Run through the safe pairs, set the next state to zero when pair has been seen.
    // Zero being the stop state, meaning we found a safe point.
    for (int32_t pairIdx=0; pairIdx<safePairs.length(); pairIdx+=2) {
        int32_t c1 = safePairs.charAt(pairIdx);
        int32_t c2 = safePairs.charAt(pairIdx + 1);

        UnicodeString &rowState = *static_cast<UnicodeString *>(fSafeTable->elementAt(c2 + 2));
        rowState.setCharAt(c1, 0);
    }

    // Remove duplicate or redundant rows from the table.
    IntPair states = {1, 0};
    while (findDuplicateSafeState(&states)) {
        // printf("Removing duplicate safe states (%d, %d)\n", states.first, states.second);
        removeSafeState(states);
    }
}


//-----------------------------------------------------------------------------
//
//   getSafeTableSize()    Calculate the size of the runtime form of this
//                         safe state table.
//
//-----------------------------------------------------------------------------
int32_t  RBBITableBuilder::getSafeTableSize() const {
    int32_t    size = 0;
    int32_t    numRows;
    int32_t    numCols;
    int32_t    rowSize;

    if (fSafeTable == nullptr) {
        return 0;
    }

    size    = offsetof(RBBIStateTable, fTableData);    // The header, with no rows to the table.

    numRows = fSafeTable->size();
    numCols = fRB->fSetBuilder->getNumCharCategories();

    if (use8BitsForSafeTable()) {
        rowSize = offsetof(RBBIStateTableRow8, fNextState) + sizeof(int8_t)*numCols;
    } else {
        rowSize = offsetof(RBBIStateTableRow16, fNextState) + sizeof(int16_t)*numCols;
    }
    size   += numRows * rowSize;
    return size;
}

bool RBBITableBuilder::use8BitsForSafeTable() const {
    return fSafeTable->size() <= kMaxStateFor8BitsTable;
}

//-----------------------------------------------------------------------------
//
//   exportSafeTable()   export the state transition table in the format required
//                       by the runtime engine.  getTableSize() bytes of memory
//                       must be available at the output address "where".
//
//-----------------------------------------------------------------------------
void RBBITableBuilder::exportSafeTable(void *where) {
    RBBIStateTable* table = static_cast<RBBIStateTable*>(where);
    uint32_t           state;
    int                col;

    if (U_FAILURE(*fStatus) || fSafeTable == nullptr) {
        return;
    }

    int32_t catCount = fRB->fSetBuilder->getNumCharCategories();
    if (catCount > 0x7fff ||
            fSafeTable->size() > 0x7fff) {
        *fStatus = U_BRK_INTERNAL_ERROR;
        return;
    }

    table->fNumStates = fSafeTable->size();
    table->fFlags     = 0;
    if (use8BitsForSafeTable()) {
        table->fRowLen    = offsetof(RBBIStateTableRow8, fNextState) + sizeof(uint8_t) * catCount;
        table->fFlags  |= RBBI_8BITS_ROWS;
    } else {
        table->fRowLen    = offsetof(RBBIStateTableRow16, fNextState) + sizeof(int16_t) * catCount;
    }

    for (state=0; state<table->fNumStates; state++) {
        UnicodeString* rowString = static_cast<UnicodeString*>(fSafeTable->elementAt(state));
        RBBIStateTableRow* row = reinterpret_cast<RBBIStateTableRow*>(table->fTableData + state * table->fRowLen);
        if (use8BitsForSafeTable()) {
            RBBIStateTableRow8* r8 = reinterpret_cast<RBBIStateTableRow8*>(row);
            r8->fAccepting = 0;
            r8->fLookAhead = 0;
            r8->fTagsIdx    = 0;
            for (col=0; col<catCount; col++) {
                U_ASSERT(rowString->charAt(col) <= kMaxStateFor8BitsTable);
                r8->fNextState[col] = static_cast<uint8_t>(rowString->charAt(col));
            }
        } else {
            row->r16.fAccepting = 0;
            row->r16.fLookAhead = 0;
            row->r16.fTagsIdx    = 0;
            for (col=0; col<catCount; col++) {
                row->r16.fNextState[col] = rowString->charAt(col);
            }
        }
    }
}




//-----------------------------------------------------------------------------
//
//   printSet    Debug function.   Print the contents of a UVector
//
//-----------------------------------------------------------------------------
#ifdef RBBI_DEBUG
void RBBITableBuilder::printSet(UVector *s) {
    int32_t  i;
    for (i=0; i<s->size(); i++) {
        const RBBINode *v = static_cast<const RBBINode *>(s->elementAt(i));
        RBBIDebugPrintf("%5d", v==nullptr? -1 : v->fSerialNum);
    }
    RBBIDebugPrintf("\n");
}
#endif


//-----------------------------------------------------------------------------
//
//   printStates    Debug Function.  Dump the fully constructed state transition table.
//
//-----------------------------------------------------------------------------
#ifdef RBBI_DEBUG
void RBBITableBuilder::printStates() {
    int     c;    // input "character"
    int     n;    // state number

    RBBIDebugPrintf("state |           i n p u t     s y m b o l s \n");
    RBBIDebugPrintf("      | Acc  LA    Tag");
    for (c=0; c<fRB->fSetBuilder->getNumCharCategories(); c++) {
        RBBIDebugPrintf(" %3d", c);
    }
    RBBIDebugPrintf("\n");
    RBBIDebugPrintf("      |---------------");
    for (c=0; c<fRB->fSetBuilder->getNumCharCategories(); c++) {
        RBBIDebugPrintf("----");
    }
    RBBIDebugPrintf("\n");

    for (n=0; n<fDStates->size(); n++) {
        RBBIStateDescriptor *sd = (RBBIStateDescriptor *)fDStates->elementAt(n);
        RBBIDebugPrintf("  %3d | " , n);
        RBBIDebugPrintf("%3d %3d %5d ", sd->fAccepting, sd->fLookAhead, sd->fTagsIdx);
        for (c=0; c<fRB->fSetBuilder->getNumCharCategories(); c++) {
            RBBIDebugPrintf(" %3d", sd->fDtran->elementAti(c));
        }
        RBBIDebugPrintf("\n");
    }
    RBBIDebugPrintf("\n\n");
}
#endif


//-----------------------------------------------------------------------------
//
//   printSafeTable    Debug Function.  Dump the fully constructed safe table.
//
//-----------------------------------------------------------------------------
#ifdef RBBI_DEBUG
void RBBITableBuilder::printReverseTable() {
    int     c;    // input "character"
    int     n;    // state number

    RBBIDebugPrintf("    Safe Reverse Table \n");
    if (fSafeTable == nullptr) {
        RBBIDebugPrintf("   --- nullptr ---\n");
        return;
    }
    RBBIDebugPrintf("state |           i n p u t     s y m b o l s \n");
    RBBIDebugPrintf("      | Acc  LA    Tag");
    for (c=0; c<fRB->fSetBuilder->getNumCharCategories(); c++) {
        RBBIDebugPrintf(" %2d", c);
    }
    RBBIDebugPrintf("\n");
    RBBIDebugPrintf("      |---------------");
    for (c=0; c<fRB->fSetBuilder->getNumCharCategories(); c++) {
        RBBIDebugPrintf("---");
    }
    RBBIDebugPrintf("\n");

    for (n=0; n<fSafeTable->size(); n++) {
        UnicodeString *rowString = (UnicodeString *)fSafeTable->elementAt(n);
        RBBIDebugPrintf("  %3d | " , n);
        RBBIDebugPrintf("%3d %3d %5d ", 0, 0, 0);  // Accepting, LookAhead, Tags
        for (c=0; c<fRB->fSetBuilder->getNumCharCategories(); c++) {
            RBBIDebugPrintf(" %2d", rowString->charAt(c));
        }
        RBBIDebugPrintf("\n");
    }
    RBBIDebugPrintf("\n\n");
}
#endif



//-----------------------------------------------------------------------------
//
//   printRuleStatusTable    Debug Function.  Dump the common rule status table
//
//-----------------------------------------------------------------------------
#ifdef RBBI_DEBUG
void RBBITableBuilder::printRuleStatusTable() {
    int32_t  thisRecord = 0;
    int32_t  nextRecord = 0;
    int      i;
    UVector  *tbl = fRB->fRuleStatusVals;

    RBBIDebugPrintf("index |  tags \n");
    RBBIDebugPrintf("-------------------\n");

    while (nextRecord < tbl->size()) {
        thisRecord = nextRecord;
        nextRecord = thisRecord + tbl->elementAti(thisRecord) + 1;
        RBBIDebugPrintf("%4d   ", thisRecord);
        for (i=thisRecord+1; i<nextRecord; i++) {
            RBBIDebugPrintf("  %5d", tbl->elementAti(i));
        }
        RBBIDebugPrintf("\n");
    }
    RBBIDebugPrintf("\n\n");
}
#endif


//-----------------------------------------------------------------------------
//
//   RBBIStateDescriptor     Methods.  This is a very struct-like class
//                           Most access is directly to the fields.
//
//-----------------------------------------------------------------------------

RBBIStateDescriptor::RBBIStateDescriptor(int lastInputSymbol, UErrorCode *fStatus) {
    fMarked    = false;
    fAccepting = 0;
    fLookAhead = 0;
    fTagsIdx   = 0;
    fTagVals   = nullptr;
    fPositions = nullptr;
    fDtran     = nullptr;

    fDtran     = new UVector32(lastInputSymbol+1, *fStatus);
    if (U_FAILURE(*fStatus)) {
        return;
    }
    if (fDtran == nullptr) {
        *fStatus = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    fDtran->setSize(lastInputSymbol+1);    // fDtran needs to be pre-sized.
                                           //   It is indexed by input symbols, and will
                                           //   hold  the next state number for each
                                           //   symbol.
}


RBBIStateDescriptor::~RBBIStateDescriptor() {
    delete       fPositions;
    delete       fDtran;
    delete       fTagVals;
    fPositions = nullptr;
    fDtran     = nullptr;
    fTagVals   = nullptr;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
