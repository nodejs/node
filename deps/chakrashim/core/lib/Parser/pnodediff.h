//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//-----------------------------------------------------------------------------
enum EditKind
{
    // No change.
    None = 0,

    // Node value was updated.
    Update,

    // Node was inserted.
    Insert,

    // Node was deleted.
    Delete,

    // Node changed parent.
    Move,

    // Node changed position within its parent. The parent nodes of the old node and the new node are matching.
    Reorder,
};

//-----------------------------------------------------------------------------
// Calculates Longest Common Subsequence.
// This uses the basic version in
//
//      EUGENE W. MYERS: An O(ND) Difference Algorithm and Its Variations
//
// The idea is that LCS is a dual problem of shortest path in an edit graph. The edit graph is a grid of lengthA
// columns and lengthB rows. A path starts from (0,0) and moves toward (lenghA, lengthB).
//  - A horizontal move (i,j) -> (i+1,j) represents deleting A[i].
//  - A vertical move (i,j) -> (i,j+1) represents inserting B[j].
//  - A diagonal move (i,j) -> (i+1,j+1) represents a match, A[i] == B[j].
// Each diagonal move represents a match. We want more diagnonal moves. Let diagonal move cost 0, horizontal or
// vertical move each costs 1. The basic algorithm is a greedy algorthm to find a shortest path from (0,0) to
// (lengthA, lengthB).
//
// Terms:
//  diagonal k: The diagnonal where x-y==k.
//  d-path: A path starting from (0,0) with d number of horizontal or vertical moves. Or, its length is d (note
//          that each horizontal/vertical move costs 1, diagnonal move costs 0).
//
//          0-path can only move along and end on diagonal 0.
//          1-path can only end on diagonal -1 or 1.
//          d-path can end on diagonal [-d, -d+2, ..., d-2, d].
//
// The basic algorithm tries to find the smallest d, where there is a d-path reaches (lengthA, lengthB).
//-----------------------------------------------------------------------------
template <class Allocator>
class LongestCommonSubsequence
{
private:
    // Stores d-path furthest reaching endpoints. They can be on diagonal [-d, -d+2, ..., d-2, d].
    class EndPoints
    {
    private:
        int d;
        int x[]; // Stores x for endpoints on the (d+1) diagonals. y == x - k.

        EndPoints(int d) : d(d)
        {
        }

    public:
        int getd() const
        {
            return d;
        }

        // Get x of furthest reaching endpoint on diagonal k.
        int operator[](int k) const
        {
            Assert(k >= -d && k <= d && (d - k) % 2 == 0); // k must be in [-d, -d+2, ..., d-2, d]
            int i = (k + d) / 2;
            return x[i];
        }

        // Get x reference of furthest reaching endpoint on diagonal k.
        int& operator[](int k)
        {
            Assert(k >= -d && k <= d && (d - k) % 2 == 0); // k must be in [-d, -d+2, ..., d-2, d]
            int i = (k + d) / 2;
            return x[i];
        }

        static EndPoints* New(Allocator* alloc, int d)
        {
            Assert(d >= 0);
            return AllocatorNewPlusLeaf(Allocator, alloc, sizeof(int) * (d + 1), EndPoints, d);
        }

        void Destroy(Allocator* alloc)
        {
            AllocatorDeletePlusLeaf(Allocator, alloc, sizeof(int) * (d + 1), this);
        }
    };

    // Represents an EditGraph for finding LCS
    class EditGraph
    {
    private:
        typedef JsUtil::List<EndPoints*, Allocator> EndPointsList;

        EndPointsList m_endPoints; // Stores endPoints for paths: -1, 0, 1, ..., d
        int m_diagonal;             // The final diagonal found on d-path that reaches destination

        // Add EndPoints storage for d-path
        EndPoints* AddEndPoints(int d)
        {
            int i = m_endPoints.Add(nullptr);
            EndPoints* e = EndPoints::New(m_endPoints.GetAllocator(), d);
            m_endPoints.Item(i, e);
            return e;
        }

    public:
        EditGraph(Allocator* alloc) : m_endPoints(alloc) {}

        ~EditGraph()
        {
            Allocator* alloc = m_endPoints.GetAllocator();
            m_endPoints.Map([=](int, EndPoints* e)
            {
                if (e)
                {
                    e->Destroy(alloc);
                }
            });
        }

        //
        // This is the basic algorithm to find a shortest path in the edit graph from (0,0) to (lengthA, lengthB).
        // We iterate through d=0,1,2,... to find smallest d, where one d-path reaches (lengthA, lengthB).
        //  - d-path must end on diagonal [-d, -d+2, ..., d-2, d]
        //  - A furthest reaching d-path on diagonal k is composed of a furthest reaching d-1 path on diagonal k-1 or k+1,
        //    followed by a vertical or horizontal move, followed by moving along diagonal k.
        //
        template <class ItemEquals>
        void FindPath(int lengthA, int lengthB, const ItemEquals& equals)
        {
            Assert(m_endPoints.Empty()); // Only support one FindPath

            int maxD;
            if (Int32Math::Add(lengthA, lengthB, &maxD) || maxD > INT_MAX / 2) // Limits maxD to simplify overflow handling
            {
                Math::DefaultOverflowPolicy();
            }

            // Pre-add virtual path -1
            {
                EndPoints& pre = *AddEndPoints(1);
                pre[1] = 0;
            }

            bool found = false;
            for (int d = 0; d <= maxD && !found; d++)
            {
                const EndPoints& v = *m_endPoints.Item(d);      // d-1 path
                EndPoints& cur = *AddEndPoints(d);              // d path

                for (int k = -d; k <= d; k += 2)
                {
                    const bool verticalMove = (k == -d || (k != d && v[k - 1] < v[k + 1]));
                    int x = verticalMove ? v[k + 1] : v[k - 1] + 1;
                    int y = x - k;

                    while (x < lengthA && y < lengthB && equals(x, y))
                    {
                        x++;
                        y++;
                    }
                    cur[k] = x; // furthest reaching end point

                    if (x == lengthA && y == lengthB)
                    {
                        m_diagonal = k;
                        found = true;
                        break;
                    }
                }
            }

            Assert(found);
        }

        template <class Func>
        void MapEdits(const Func& map) const
        {
            // m_endPoints contains endPoints for paths: -1, 0, 1, ..., d
            int d = m_endPoints.Count() - 2;
            int k = m_diagonal;

            for (; d >= 0; d--)
            {
                const EndPoints& v = *m_endPoints.Item(d);          // d-1 path
                const EndPoints& cur = *m_endPoints.Item(d + 1);    // d path
                Assert(cur.getd() == d);

                const bool verticalMove = (k == -d || (k != d && v[k - 1] < v[k + 1]));
                int x0 = verticalMove ? v[k + 1] : v[k - 1] + 1;

                int x = cur[k];
                int y = x - k;
                while (x > x0)
                {
                    map(EditKind::Update, --x, --y);
                }

                if (verticalMove)
                {
                    if (d > 0) // Don't emit virtual initial move from path -1 to 0
                    {
                        map(EditKind::Insert, -1, --y);
                    }
                    k++;
                }
                else
                {
                    map(EditKind::Delete, --x, -1);
                    k--;
                }
            }
        }
    };

    struct Edit
    {
        EditKind kind;
        int indexA;
        int indexB;

        Edit() {}
        Edit(EditKind kind, int indexA, int indexB) :
            kind(kind), indexA(indexA), indexB(indexB)
        {
            Assert((kind == EditKind::Insert && indexA == -1 && indexB >= 0)
                || (kind == EditKind::Delete && indexA >= 0 && indexB == -1)
                || (kind == EditKind::Update && indexA >= 0 && indexB >= 0));
        }
    };

    typedef JsUtil::List<Edit, Allocator, /*isLeaf*/true> EditList;

    EditList m_edits;

public:
    template <class ItemEquals>
    LongestCommonSubsequence(Allocator* alloc, int lengthA, int lengthB, const ItemEquals& equals) :
        m_edits(alloc)
    {
        EditGraph graph(alloc);
        graph.FindPath(lengthA, lengthB, equals);
        graph.MapEdits([this](EditKind kind, int indexA, int indexB)
        {
            m_edits.Add(Edit(kind, indexA, indexB));
        });
    }

    template <class Func>
    void MapEdits(const Func& map) const
    {
        for (int i = m_edits.Count() - 1; i >= 0; i--)
        {
            const Edit& e = m_edits.Item(i);
            map(e.kind, e.indexA, e.indexB);
        }
    }

    template <class Func>
    void MapMatches(const Func& map) const
    {
        MapEdits([&](EditKind kind, int indexA, int indexB)
        {
            if (kind == EditKind::Update)
            {
                map(indexA, indexB);
            }
        });
    }
};

//
// Returns a distance [0..1] of the specified sequences. The smaller distance the more of their elements match.
//
template <class Allocator, class ItemEquals>
double ComputeLongestCommonSubsequenceDistance(Allocator* alloc, int lengthA, int lengthB, const ItemEquals& equals)
{
    Assert(lengthA >= 0 && lengthB >= 0);
    if (lengthA == 0 || lengthB == 0)
    {
        return (lengthA == lengthB) ? 0.0 : 1.0;
    }

    int lcsLength = 0;
    LongestCommonSubsequence<Allocator> lcs(alloc, lengthA, lengthB, equals);
    lcs.MapMatches([&](int, int)
    {
        ++lcsLength;
    });

    return 1.0 - (double)lcsLength / (double)max(lengthA, lengthB);
}

//-----------------------------------------------------------------------------
// Base class for TreeComparers, used with TreeMatch. TreeComparers specify parse node details.
//-----------------------------------------------------------------------------
template <class SubClass, class Node>
struct TreeComparerBase
{
    typedef Node Node;
    typedef Node* PNode;

    static const double ExactMatchDistance;
    static const double EpsilonDistance;

    const SubClass* pThis() const { return static_cast<const SubClass*>(this); }
    SubClass* pThis() { return static_cast<SubClass*>(this); }

    // The number of distinct labels used in the tree.
    int LabelCount() const { return 0; }

    // Returns an integer label corresponding to the given node.
    // Returned value must be within [0, LabelCount).
    int GetLabel(PNode x) const { return 0; }

    // Returns N > 0 if the node with specified label can't change its N-th ancestor node, zero otherwise.
    // 1st ancestor is the node's parent node.
    // 2nd ancestor is the node's grandparent node.
    // etc.
    int TiedToAncestor(int label) { return 0; }

    // Calculates the distance [0..1] of two nodes.
    // The more similar the nodes the smaller the distance.
    //
    // Used to determine whether two nodes of the same label match.
    // Even if 0 is returned the nodes might be slightly different.
    double GetDistance(PNode x, PNode y) const { return 0; }

    // Returns true if the specified nodes have equal values.
    // Called with matching nodes (oldNode, newNode).
    // Return true if the values of the nodes are the same, or their difference is not important.
    bool ValuesEqual(PNode oldNode, PNode newNode) const { return true; }

    PNode GetParent(PNode x) const { return nullptr; }

    bool TryGetParent(PNode x, _Out_ PNode* p) const
    {
        *p = pThis()->GetParent(x);
        return *p != nullptr;
    }

    PNode GetAncestor(PNode node, int level) const
    {
        while (level > 0)
        {
            node = pThis()->GetParent(node);
            level--;
        }

        return node;
    }

    // Map children nodes of x
    template <class Func>
    void MapChildren(PNode x, const Func& func) const {}

    // Map all descendant nodes of x (not including x itself)
    template <class Func>
    void MapDescendants(PNode x, const Func& func) const
    {
        pThis()->MapChildren(x, [&](PNode child)
        {
            func(child);
            MapDescendants(child, func);
        });
    }

    // Map every node in the (sub)tree x.
    template <class Func>
    void MapTree(PNode x, const Func& func) const
    {
        func(x);
        pThis()->MapDescendants(x, func);
    }

    // Return true if specified nodes belong to the same tree. For debug only.
    bool TreesEqual(PNode left, PNode right) const { return true; }
};

template <class SubClass, class Node> const double TreeComparerBase<SubClass, Node>::ExactMatchDistance = 0.0;
template <class SubClass, class Node> const double TreeComparerBase<SubClass, Node>::EpsilonDistance = 0.00001;

//-----------------------------------------------------------------------------
// Tree match algorithm, based on general algorithm described in
//      Change Detection in Hierarchically Structured Information
//      by Sudarshan S. Chawathe, Anand Rajaraman, Hector Garcia-Molina, and Jennifer Widom
//
// Derived from Roslyn implementation.
//-----------------------------------------------------------------------------
template <class TreeComparer, class Allocator>
class TreeMatch
{
public:
    // ParseNodes are owned by Parser arena. Considered leaf here.
    typedef typename TreeComparer::PNode PNode;
    typedef JsUtil::List<PNode, Allocator, /*isLeaf*/true> NodeList;
    typedef JsUtil::BaseDictionary<PNode, PNode, typename ForceLeafAllocator<Allocator>::AllocatorType> NodeMap;

private:
    static const double ExactMatchDistance;
    static const double EpsilonDistance;
    static const double MatchingDistance1;
    static const double MatchingDistance2;
    static const double MatchingDistance3;
    static const double MaxDistance;

    Allocator* alloc;
    const PNode root1;
    const PNode root2;
    TreeComparer comparer;

    NodeMap* oneToTwo;
    NodeMap* twoToOne;

public:
    TreeMatch(Allocator* alloc, PNode root1, PNode root2, const TreeComparer& comparer = TreeComparer()) :
        alloc(alloc), root1(root1), root2(root2), comparer(comparer)
    {
        const int labelCount = comparer.LabelCount();

        // calculate chains (not including root node)
        AutoAllocatorObjectArrayPtr<NodeList, Allocator> nodes1(AllocatorNewArrayZ(Allocator, alloc, NodeList*, labelCount), labelCount, alloc);
        AutoAllocatorObjectArrayPtr<NodeList, Allocator> nodes2(AllocatorNewArrayZ(Allocator, alloc, NodeList*, labelCount), labelCount, alloc);
        int count1 = CategorizeNodesByLabels(root1, labelCount, nodes1);
        int count2 = CategorizeNodesByLabels(root2, labelCount, nodes2);

        AutoAllocatorObjectPtr<NodeMap, Allocator> map1(AllocatorNew(Allocator, alloc, NodeMap, alloc, count1), alloc);
        AutoAllocatorObjectPtr<NodeMap, Allocator> map2(AllocatorNew(Allocator, alloc, NodeMap, alloc, count2), alloc);
        this->oneToTwo = map1;
        this->twoToOne = map2;

        ComputeMatch(nodes1, nodes2, labelCount);

        // Succeeded. Detach local objects that are now owned by this instance.
        map1.Detach();
        map2.Detach();
    }

    ~TreeMatch()
    {
        DeleteObject<Allocator>(alloc, oneToTwo);
        DeleteObject<Allocator>(alloc, twoToOne);
    }

    const TreeComparer& Comparer() const { return comparer; }
    PNode OldRoot() const { return root1;  }
    PNode NewRoot() const { return root2; }

    bool HasPartnerInTree1(PNode node2) const
    {
        Assert(comparer.TreesEqual(node2, root2));
        return twoToOne->ContainsKey(node2);
    }

    bool HasPartnerInTree2(PNode node1) const
    {
        Assert(comparer.TreesEqual(node1, root1));
        return oneToTwo->ContainsKey(node1);
    }

    bool TryGetPartnerInTree1(PNode node2, PNode* partner1) const
    {
        Assert(comparer.TreesEqual(node2, root2));
        return twoToOne->TryGetValue(node2, partner1);
    }

    bool TryGetPartnerInTree2(PNode node1, PNode* partner2) const
    {
        Assert(comparer.TreesEqual(node1, root1));
        return oneToTwo->TryGetValue(node1, partner2);
    }

    bool Contains(PNode node1, PNode node2) const
    {
        Assert(comparer.TreesEqual(node2, root2));

        PNode partner2;
        return TryGetPartnerInTree2(node1, &partner2) && node2 == partner2;
    }

private:
    int CategorizeNodesByLabels(PNode root, int labelCount, _Out_writes_(labelCount) NodeList* nodes[])
    {
        int count = 0;
        comparer.MapDescendants(root, [&](PNode node)
        {
            int label = comparer.GetLabel(node);
            Assert(label >= 0 && label < labelCount);

            NodeList* list = nodes[label];
            if (!list)
            {
                list = NodeList::New(alloc);
                nodes[label] = list;
            }

            list->Add(node);
            count++;
        });

        return count;
    }

    void ComputeMatch(_In_reads_(labelCount) NodeList* nodes1[], _In_reads_(labelCount) NodeList* nodes2[], int labelCount)
    {
        // Root nodes always match but they might have been added as knownMatches
        if (!HasPartnerInTree2(root1))
        {
            Add(root1, root2);
        }

        // --- The original FastMatch algorithm ---
        //
        // For each leaf label l, and then for each internal node label l do:
        // a) S1 := chain T1(l)
        // b) S2 := chain T2(l)
        // c) lcs := LCS(S1, S2, Equal)
        // d) For each pair of nodes (x,y) in lcs add (x,y) to M.
        // e) Pair unmatched nodes with label l as in Algorithm Match, adding matches to M:
        //    For each unmatched node x in T1, if there is an unmatched node y in T2 such that equal(x,y)
        //    then add (x,y) to M.
        //
        // equal(x,y) is defined as follows:
        //   x, y are leafs => equal(x,y) := label(x) == label(y) && compare(value(x), value(y)) <= f
        //   x, y are nodes => equal(x,y) := label(x) == label(y) && |common(x,y)| / max(|x|, |y|) > t
        // where f, t are constants.
        //
        // --- Actual implementation ---
        //
        // We also categorize nodes by their labels, but then we proceed differently:
        //
        // 1) A label may be marked "tied to parent". Let x, y have both label l and l is "tied to parent".
        //    Then (x,y) can be in M only if (parent(x), parent(y)) in M.
        //    Thus we require labels of children tied to a parent to be preceeded by all their possible parent labels.
        //
        // 2) Rather than defining function equal in terms of constants f and t, which are hard to get right,
        //    we try to match multiple times with different threashold for node distance.
        //    The comparer defines the distance [0..1] between two nodes and it can do so by analyzing
        //    the node structure and value. The comparer can tune the distance specifically for each node kind.
        //    We first try to match nodes of the same labels to the exactly matching or almost matching counterpars.
        //    The we keep increasing the threashold and keep adding matches.
        for (int label = 0; label < labelCount; label++)
        {
            if (nodes1[label] && nodes2[label])
            {
                ComputeMatchForLabel(label, *nodes1[label], *nodes2[label]);
            }
        }
    }

    void ComputeMatchForLabel(int label, NodeList& s1, NodeList& s2)
    {
        int tiedToAncestor = comparer.TiedToAncestor(label);

        ComputeMatchForLabel(s1, s2, tiedToAncestor, EpsilonDistance);     // almost exact match
        ComputeMatchForLabel(s1, s2, tiedToAncestor, MatchingDistance1);   // ok match
        ComputeMatchForLabel(s1, s2, tiedToAncestor, MatchingDistance2);   // ok match
        ComputeMatchForLabel(s1, s2, tiedToAncestor, MatchingDistance3);   // ok match
        ComputeMatchForLabel(s1, s2, tiedToAncestor, MaxDistance);         // any match
    }

    void ComputeMatchForLabel(NodeList& s1, NodeList& s2, int tiedToAncestor, double maxAcceptableDistance)
    {
        // Obviously, the algorithm below is O(n^2). However, in the common case, the 2 lists will
        // be sequences that exactly match. The purpose of "firstNonMatch2" is to reduce the complexity
        // to O(n) in this case. Basically, the pointer is the 1st non-matched node in the list of nodes of tree2
        // with the given label.
        // Whenever we match to firstNonMatch2 we set firstNonMatch2 to the subsequent node.
        // So in the case of totally matching sequences, we process them in O(n) -
        // both node1 and firstNonMatch2 will be advanced simultaneously.

        UnmatchedIterator i1(s1);
        for (;;)
        {
            PNode node1 = i1.GetNextUnmatched();
            if (!node1) break;
            Assert(!HasPartnerInTree2(node1));

            // Find node2 that matches node1 the best, i.e. has minimal distance.

            double bestDistance = MaxDistance;
            PNode bestMatch = nullptr;
            int bestMatchIndex = -1; // node1's best match index in list2
            bool matched = false;
            UnmatchedIterator i2(s2);

            for (;;)
            {
                PNode node2 = i2.GetNextUnmatched();
                if (!node2) break;
                Assert(!HasPartnerInTree1(node2));

                // this requires parents to be processed before their children:
                if (tiedToAncestor > 0)
                {
                    // TODO: For nodes tied to their parents,
                    // consider avoding matching them to all other nodes of the same label.
                    // Rather we should only match them with their siblings that share the same parent.

                    PNode ancestor1 = comparer.GetAncestor(node1, tiedToAncestor);
                    PNode ancestor2 = comparer.GetAncestor(node2, tiedToAncestor);
                    Assert(comparer.GetLabel(ancestor1) < comparer.GetLabel(node1));

                    if (!Contains(ancestor1, ancestor2))
                    {
                        continue;
                    }
                }

                // We know that
                // 1. (node1, node2) not in M
                // 2. Both of their parents are matched to the same parent (or are not matched)
                //
                // Now, we have no other choice than comparing the node "values"
                // and looking for the one with the smaller distance.
                //
                double distance = comparer.GetDistance(node1, node2);
                if (distance < bestDistance)
                {
                    matched = true;
                    bestMatch = node2;
                    bestMatchIndex = i2.CurIndex();
                    bestDistance = distance;

                    // We only stop if we've got an exact match. This is to resolve the problem
                    // of entities with identical names(name is often used as the "value" of a
                    // node) but with different "sub-values" (e.g. two locals may have the same name
                    // but different types. Since the type is not part of the value, we don't want
                    // to stop looking for the best match if we don't have an exact match).
                    if (distance == ExactMatchDistance)
                    {
                        break;
                    }
                }
            }

            if (matched && bestDistance <= maxAcceptableDistance)
            {
                Add(node1, bestMatch);

                i1.MarkCurrentMatched();        // i1's match is current node1
                i2.MarkMatched(bestMatchIndex); // i2's match is one of the nodes examined in the above for(;;) pass
            }
        }
    }

    void Add(PNode node1, PNode node2)
    {
        Assert(comparer.TreesEqual(node1, root1));
        Assert(comparer.TreesEqual(node2, root2));

        oneToTwo->Add(node1, node2);
        twoToOne->Add(node2, node1);
    }

    // The customized Match algorithm iterates over the 2 node lists, compares every unmatched node pair to match nodes.
    // To find the next unmatched node, original algorithm iterates over every node in each list, use a dictionary lookup
    // to test if the node has been matched or not, until it sees next unmatched node. This could be very expensive if the
    // lists are huge. E.g., assume the only diff is inserting a new node at the beginning of list2. Then for each node in
    // list1, it checks every node starting from the beginning new node in list2 for next unmatched node. This results in
    // O(N^2) dictionary lookups. And we do 5 passes of these.
    //
    // To improve on this, we can try to record every match span and directly jump to next unmatched position. Note that
    // in both lists once a node is matched, the list entry is no longer used. We can reuse that space to record extra info.
    //      * Original PNode pointer value must be at even address. The list item must have 0 at bit0 (lowest bit).
    //      * Once a node is matched, mark 1 at bit0. With this we can get rid of dictionary lookup.
    //      * Next, for each matched entry, use the upper bits to record "next" unmatched index. Try to maintain match span,
    //        so that from a matched node we can directly jump to next unmatched index.
    //
    // This class is for above purpose. Expected call pattern:
    //      * GetNextUnmatched, [MarkCurrentMatched], GetNextUnmatched, [MarkCurrentMatched], ...
    //          -- (A) With first MarkCurrentMatched we know the start of a match span.
    //          -- (B) Subsequent MarkCurrentMatched indicates continuous match span.
    //          -- (C) When MarkCurrentMatched is not called for an entry, we know the end of a match span. Record the whole
    //             span (A)->(C). If walked again we would directly jump from (A) to (C).
    //      * Random MarkMatched(i)
    //          -- We don't know the exact match span. Just mark this entry "i" as matched, but set its "next" (upper bits) to 0.
    //          -- During next pass, we can merge all adjacent match spans and individual matched entries to bigger match spans.
    //             This would help next pass (we have 5).
    //
    class UnmatchedIterator
    {
    private:
        NodeList& list;
        int lastMatched;    // last matched node index. -1 means no known last matched index.
        int index;          // current examining index. Only moved by GetNextUnmatched().

    public:
        UnmatchedIterator(NodeList& list) :
            list(list),
            lastMatched(-1),
            index(-1)
        {
            VerifySize(list);
        }

        ~UnmatchedIterator()
        {
            // If we have lastMatched, we could have one of following:
            //      * index is matched by MarkCurrentMatched(). Link lastMatched -> index (== lastMatched). GetNextUnmatched() can handle it.
            //      * index remains unmatched (ends a matched sequence). Link lastMatched -> index.
            //      * index is out of range. That means [lastMatched, ...end) are all matched. Link lastMatched -> index (out of range).
            //
            if (lastMatched >= 0)
            {
                SetNext(lastMatched, index);
            }
        }

        PNode GetNextUnmatched()
        {
            // If current ends a matched sequence, make a link [lastMatched -> current).
            if (lastMatched >= 0 && !IsMatched(index))
            {
                SetNext(lastMatched, index);
                lastMatched = -1;
            }

            ++index;
            if (index < list.Count())
            {
                if (IsMatched(index))
                {
                    if (lastMatched < 0) // Check if current starts a matched sequence
                    {
                        lastMatched = index;
                    }

                    // Jumps all matched span, until sees an unmatched entry or the end.
                    int next;
                    while (index < list.Count() && IsNext(list.Item(index), &next))
                    {
                        index = max(next, index + 1); // Ensure moves forward (next could be 0, from individual MarkMatched() call).
                    }
                }

                if (index < list.Count())
                {
                    return list.Item(index);
                }
            }

            return nullptr;
        }

        int CurIndex() const { return index; }

        void MarkMatched(int i)
        {
            if (i == index)
            {
                MarkCurrentMatched();
            }
            else
            {
                SetMatched(i);
            }
        }

        void MarkCurrentMatched()
        {
            Assert(!IsMatched(index));
            SetMatched(index);

            if (lastMatched < 0) // If current starts a matched sequence
            {
                lastMatched = index;
            }
        }

    private:
        static void VerifySize(const NodeList& list)
        {
            if (list.Count() > INT_MAX / 2) // Limit max size as we used bit0
            {
                Math::DefaultOverflowPolicy();
            }
        }

        static void SetMatched(PNode& node)
        {
            SetNext(node, 0);
        }

        static bool IsMatched(PNode node)
        {
            return !!(reinterpret_cast<UINT_PTR>(node) & 1);
        }

        static void SetNext(PNode& node, int next)
        {
            UINT_PTR value = (static_cast<UINT_PTR>(next) << 1) | 1;
            node = reinterpret_cast<PNode>(value);
        }

        static bool IsNext(PNode node, _Out_ int* next)
        {
            UINT_PTR value = reinterpret_cast<UINT_PTR>(node);
            if (value & 1)
            {
                *next = static_cast<int>(value >> 1);
                return true;
            }

            return false;
        }

        void SetMatched(int i) { SetMatched(list.Item(i)); }
        bool IsMatched(int i) const { return IsMatched(list.Item(i)); }
        void SetNext(int i, int next) { SetNext(list.Item(i), next); }
        bool IsNext(int i, _Out_ int* next) const { return IsNext(list.Item(i), next); }
    };
};

template <class TreeComparer, class Allocator> const double TreeMatch<TreeComparer, Allocator>::ExactMatchDistance = TreeComparer::ExactMatchDistance;
template <class TreeComparer, class Allocator> const double TreeMatch<TreeComparer, Allocator>::EpsilonDistance = TreeComparer::EpsilonDistance;
template <class TreeComparer, class Allocator> const double TreeMatch<TreeComparer, Allocator>::MatchingDistance1 = 0.5;
template <class TreeComparer, class Allocator> const double TreeMatch<TreeComparer, Allocator>::MatchingDistance2 = 1.0;
template <class TreeComparer, class Allocator> const double TreeMatch<TreeComparer, Allocator>::MatchingDistance3 = 1.5;
template <class TreeComparer, class Allocator> const double TreeMatch<TreeComparer, Allocator>::MaxDistance = 2.0;

//-----------------------------------------------------------------------------
// Represents an edit operation on a tree or a sequence of nodes.
//-----------------------------------------------------------------------------
template <class PNode>
class Edit
{
private:
    EditKind kind;
    PNode node1;
    PNode node2;

public:
    Edit() {}

    //
    //  Insert      nullptr    NewNode
    //  Delete      OldNode nullptr
    //  Move/Update OldNode NewNode
    //
    Edit(EditKind kind, PNode node1, PNode node2) :
        kind(kind), node1(node1), node2(node2)
    {
        Assert((node1 == nullptr) == (kind == EditKind::Insert));
        Assert((node2 == nullptr) == (kind == EditKind::Delete));
    }

    EditKind Kind() const { return kind; }
    PNode OldNode() const { return node1; }
    PNode NewNode() const { return node2; }
};

//-----------------------------------------------------------------------------
// Represents a sequence of tree edits.
//-----------------------------------------------------------------------------
template <class TreeComparer, class Allocator>
class EditScript
{
public:
    typedef TreeMatch<TreeComparer, Allocator> TreeMatch;
    typedef typename TreeMatch::PNode PNode;
    typedef typename TreeMatch::NodeList NodeList;
    typedef typename TreeMatch::NodeMap NodeMap;
    typedef JsUtil::List<Edit<PNode>, Allocator, /*isLeaf*/true> EditList;

private:
    const TreeMatch& match;
    TreeComparer comparer;
    EditList edits;

public:
    EditScript(Allocator* alloc, const TreeMatch& match) :
        match(match), comparer(match.Comparer()), edits(alloc)
    {
        AddUpdatesInsertsMoves();
        AddDeletes();
    }

    const EditList& Edits() const { return edits; }

private:
    PNode Root1() const { return match.OldRoot(); }
    PNode Root2() const { return match.NewRoot(); }

    void AddUpdatesInsertsMoves()
    {
        // Breadth-first traversal.
        ProcessNode(Root2());

        JsUtil::Queue<PNode, Allocator> queue(edits.GetAllocator());
        queue.Enqueue(Root2());

        while (!queue.Empty())
        {
            PNode head = queue.Dequeue();
            comparer.MapChildren(head, [&](PNode child)
            {
                ProcessNode(child);
                queue.Enqueue(child);
            });
        }
    }

    void ProcessNode(PNode x)
    {
        Assert(comparer.TreesEqual(x, Root2()));

        // NOTE:
        // Our implementation differs from the algorithm described in the paper in following:
        // - We don't update M' and T1 since we don't need the final matching and the transformed tree.
        // - Insert and Move edits don't need to store the offset of the nodes relative to their parents,
        //   so we don't calculate those. Thus we don't need to implement FindPos.
        // - We don't mark nodes "in order" since the marks are only needed by FindPos.

        // a)
        // Let x be the current node in the breadth-first search of T2.
        // Let y = parent(x).
        // Let z be the partner of parent(x) in M'.  (note: we don't need z for insert)
        //
        // NOTE:
        // If we needed z then we would need to be updating M' as we encounter insertions.

        PNode w;
        bool hasPartner = match.TryGetPartnerInTree1(x, &w);

        PNode y;
        bool hasParent = comparer.TryGetParent(x, &y);

        if (!hasPartner)
        {
            // b) If x has no partner in M'.
            //   i. k := FindPos(x)
            //  ii. Append INS((w, a, value(x)), z, k) to E for a new identifier w.
            // iii. Add (w, x) to M' and apply INS((w, a, value(x)), z, k) to T1.
            edits.Add(Edit<PNode>(EditKind::Insert, /*node1*/nullptr, /*node2*/x));

            // NOTE:
            // We don't update M' here.
        }
        else if (hasParent)
        {
            // c) else if x is not a root
            // i. Let w be the partner of x in M', and let v = parent(w) in T1.
            PNode v = comparer.GetParent(w);

            // ii. if value(w) != value(x)
            // A. Append UPD(w, value(x)) to E
            // B. Apply UPD(w, value(x) to T1

            // Let the Comparer decide whether an update should be added to the edit list.
            // The Comparer defines what changes in node values it cares about.
            if (!comparer.ValuesEqual(w, x))
            {
                edits.Add(Edit<PNode>(EditKind::Update, /*node1*/w, /*node2*/x));
            }

            // If parents of w and x don't match, it's a move.
            // iii. if not (v, y) in M'
            // NOTE: The paper says (y, v) but that seems wrong since M': T1 -> T2 and w,v in T1 and x,y in T2.
            if (!match.Contains(v, y))
            {
                // A. Let z be the partner of y in M'. (NOTE: z not needed)
                // B. k := FindPos(x)
                // C. Append MOV(w, z, k)
                // D. Apply MOV(w, z, k) to T1
                edits.Add(Edit<PNode>(EditKind::Move, /*node1*/w, /*node2*/x));
            }
        }

        // d) AlignChildren(w, x)

        // NOTE: If we just applied an INS((w, a, value(x)), z, k) operation on tree T1
        // the newly created node w would have no children. So there is nothing to align.
        if (hasPartner)
        {
            AlignChildren(w, x);
        }
    }

    void AddDeletes()
    {
        // 3. Do a post-order traversal of T1.
        //    a) Let w be the current node in the post-order traversal of T1.
        //    b) If w has no partner in M' then append DEL(w) to E and apply DEL(w) to T1.
        //
        // NOTE: The fact that we haven't updated M' during the Insert phase
        // doesn't affect Delete phase. The original algorithm inserted new node n1 into T1
        // when an insertion INS(n1, n2) was detected. It also added (n1, n2) to M'.
        // Then in Delete phase n1 is visited but nothing is done since it has a partner n2 in M'.
        // Since we don't add n1 into T1, not adding (n1, n2) to M' doesn't affect the Delete phase.

        comparer.MapDescendants(Root1(), [&](PNode w)
        {
            if (!match.HasPartnerInTree2(w))
            {
                edits.Add(Edit<PNode>(EditKind::Delete, /*node1*/w, /*node2*/nullptr));
            }
        });
    }

    void AlignChildren(PNode w, PNode x)
    {
        Assert(comparer.TreesEqual(w, Root1()));
        Assert(comparer.TreesEqual(x, Root2()));
        Allocator* alloc = edits.GetAllocator();

        // Step 1
        //  Make all children of w and and all children x "out of order"
        //  NOTE: We don't need to mark nodes "in order".

        // Step 2
        //  Let S1 be the sequence of children of w whose partner are children
        //  of x and let S2 be the sequence of children of x whose partner are
        //  children of w.
        NodeList s1(alloc), s2(alloc);
        if (!TryGetMatchedChildren(s1, w, x, [&](PNode e, PNode* partner) { return match.TryGetPartnerInTree2(e, partner); }) ||
            !TryGetMatchedChildren(s2, x, w, [&](PNode e, PNode* partner) { return match.TryGetPartnerInTree1(e, partner); }))
        {
            return;
        }

        // Step 3, 4
        //  Define the function Equal(a,b) to be true if and only if  (a,b) in M'
        //  Let S <- LCS(S1, S2, Equal)
        NodeMap s(alloc);
        {
            LongestCommonSubsequence<Allocator> lcs(alloc, s1.Count(), s2.Count(), [&](int indexA, int indexB)
            {
                return match.Contains(s1.Item(indexA), s2.Item(indexB));
            });

            lcs.MapMatches([&](int indexA, int indexB)
            {
                s.AddNew(s1.Item(indexA), s2.Item(indexB));
            });
        }

        // Step 5
        //  For each (a,b) in S, mark nodes a and b "in order"
        //  NOTE: We don't need to mark nodes "in order".

        // Step 6
        //  For each a in S1, b in S2 such that (a,b) in M but (a,b) not in S
        //   (a) k <- FindPos(b)
        //   (b) Append MOV(a,w,k) to E and apply MOV(a,w,k) to T1
        //   (c) Mark a and b "in order"
        //       NOTE: We don't mark nodes "in order".
        s1.Map([&](int index, PNode a)
        {
            PNode b;
            if (match.TryGetPartnerInTree2(a, &b)   // (a,b) in M
                && comparer.GetParent(b) == x       // => b in S2 since S2 == { b | parent(b) == x && parent(partner(b)) == w }
                && !ContainsPair(s, a, b))          // (a,b) not in S
            {
                Assert(comparer.TreesEqual(a, Root1()));
                Assert(comparer.TreesEqual(b, Root2()));

                edits.Add(Edit<PNode>(EditKind::Reorder, /*node1*/a, /*node2*/b));
            }
        });
    }

    // Helper: Get the sequence of children of x whose partner are children of y.
    template <class TryGetPartnerFunc>
    bool TryGetMatchedChildren(NodeList& nodes, PNode x, PNode y, const TryGetPartnerFunc& tryGetPartner)
    {
        Assert(nodes.Empty());
        comparer.MapChildren(x, [&](PNode e)
        {
            PNode partner;
            if (tryGetPartner(e, &partner) && comparer.GetParent(partner) == y)
            {
                nodes.Add(e);
            }
        });
        return !nodes.Empty();
    }

    static bool ContainsPair(const NodeMap& dict, PNode a, PNode b)
    {
        PNode value;
        return dict.TryGetValue(a, &value) && value == b;
    }
};

