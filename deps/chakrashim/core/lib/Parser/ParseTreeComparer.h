//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#ifdef EDIT_AND_CONTINUE

namespace Js
{
    class SyntaxEquivalenceBase;
    template <class Allocator> class SyntaxEquivalence;

    //-----------------------------------------------------------------------------
    // TreeComparer for ParseNode TreeMatch.
    //-----------------------------------------------------------------------------
    template <class SubClass, class Allocator>
    class ParseTreeComparer : public TreeComparerBase<SubClass, ParseNode>
    {
    private:
        static const int TOKENLIST_MAXDIFF_SHIFT = 3; // Used to detect lists of significantly different lengths

        SyntaxEquivalence<Allocator> syntaxEquivalence;

        // 2 lists used in GetDistance. (Can mark isLeaf because they don't own the nodes.)
        typedef JsUtil::List<PNode, Allocator, /*isLeaf*/true> NodeList;
        NodeList leftList, rightList;

    public:
        ParseTreeComparer(Allocator* alloc) :
            syntaxEquivalence(alloc), leftList(alloc), rightList(alloc)
        {}

        ParseTreeComparer(const ParseTreeComparer& other) :
            syntaxEquivalence(other.GetAllocator()), leftList(other.GetAllocator()), rightList(other.GetAllocator())
        {}

        Allocator* GetAllocator() const
        {
            return leftList.GetAllocator();
        }

        int LabelCount() const
        {
            return ::OpCode::knopLim;
        }

        int GetLabel(PNode x) const
        {
            return x->nop;
        }

        PNode GetParent(PNode x) const
        {
            return x->parent;
        }

        template <class Func>
        void MapChildren(PNode x, const Func& func) const
        {
            Js::MapChildren(x, func);
        }

        // Map (sub)tree nodes to compute distance. Child class can re-implement to control which nodes participate in
        // distance computing.
        template <class Func>
        void MapTreeToComputeDistance(PNode x, const Func& func) const
        {
            pThis()->MapTree(x, func);
        }

        double GetDistance(PNode left, PNode right)
        {
            Assert(pThis()->GetLabel(left) == pThis()->GetLabel(right)); // Only called for nodes of same label
            return ComputeValueDistance(left, right);
        }

        bool ValuesEqual(PNode oldNode, PNode newNode)
        {
            // This determines if we emit Update edit for matched nodes. If ValuesEqual, don't need update edit.
            return !(syntaxEquivalence.IsToken(oldNode) || syntaxEquivalence.HasToken(oldNode))
                || syntaxEquivalence.AreEquivalent(oldNode, newNode);
        }

    private:
        double ComputeValueDistance(PNode left, PNode right)
        {
            // If 2 nodes are equivalent trees, consider them exact match.
            if (syntaxEquivalence.AreEquivalent(left, right))
            {
                return ExactMatchDistance;
            }

            double distance = ComputeDistance(left, right);

            // We don't want to return an exact match, because there
            // must be something different, since we got here
            return (distance == ExactMatchDistance) ? EpsilonDistance : distance;
        }

        //
        // Computer distance the same as Roslyn:
        //  * For token nodes, use their string LCS distance.
        //  * Otherwise, flatten the tree to get all tokens, use token list LCS distance.
        //
        // However, our parser are significantly different to Roslyn. Roslyn uses "full fidelity" parser,
        // keeping every token scanned from source. e.g., "var a = 1" -> "var","a","=","1". Our parser keeps
        // much less tokens. Thus our LCS distance will be quite different, which may affect diff accuracy.
        //
        double ComputeDistance(PNode left, PNode right)
        {
            // For token nodes, use their string LCS distance
            if (syntaxEquivalence.IsToken(left))
            {
                return ComputeTokenDistance(left, right);
            }

            // Otherwise, flatten the tree to get all tokens, use token list LCS distance
            Flatten(left, leftList);
            Flatten(right, rightList);

            // If token list lengths are significantly different, consider they are quite different.
            {
                int leftLen = leftList.Count();
                int rightLen = rightList.Count();
                int minLen = min(leftLen, rightLen);
                int maxLen = max(leftLen, rightLen);
                if (minLen < (maxLen >> TOKENLIST_MAXDIFF_SHIFT))
                {
                    // Assuming minLen are all matched, distance > 0.875 (7/8). These two nodes shouldn't be a match.
                    return 1.0 - (double)minLen / (double)maxLen;
                }
            }

            return ComputeLongestCommonSubsequenceDistance(GetAllocator(), leftList.Count(), rightList.Count(), [this](int indexA, int indexB)
            {
                return AreNodesTokenEquivalent(leftList.Item(indexA), rightList.Item(indexB));
            });
        }

        // Flatten IsToken/HasToken nodes in the (sub)tree into given list to compute distance.
        void Flatten(PNode root, NodeList& list)
        {
            list.Clear();

            pThis()->MapTreeToComputeDistance(root, [&](PNode child)
            {
                if (syntaxEquivalence.IsToken(child) || syntaxEquivalence.HasToken(child))
                {
                    list.Add(child);
                }
            });
        }

        // Check if IsToken/HasToken nodes are equivalent
        bool AreNodesTokenEquivalent(PNode left, PNode right)
        {
            if (left->nop == right->nop)
            {
                return syntaxEquivalence.IsToken(left) ?
                    syntaxEquivalence.AreTokensEquivalent(left, right) : syntaxEquivalence.HaveEquivalentTokens(left, right);
            }

            return false;
        }

        double ComputeTokenDistance(PNode left, PNode right) const
        {
            Assert(syntaxEquivalence.IsToken(left));
            switch (left->nop)
            {
            case knopName:
            case knopStr:
                return ComputeDistance(left->sxPid.pid, right->sxPid.pid);

            case knopInt:
                return left->sxInt.lw == right->sxInt.lw ? ExactMatchDistance : 1.0;

            case knopFlt:
                return left->sxFlt.dbl == right->sxFlt.dbl ? ExactMatchDistance : 1.0;

            case knopRegExp: //TODO: sxPid.regexPattern
                break;
            }

            // Other token nodes with fixed strings, e.g. "true", "null", always match exactly
            return ExactMatchDistance;
        }

        // Compute distance of 2 PIDs as their string LCS distance
        double ComputeDistance(IdentPtr left, IdentPtr right) const
        {
            Allocator* alloc = leftList.GetAllocator();
            return ComputeLongestCommonSubsequenceDistance(alloc, left->Cch(), right->Cch(), [=](int indexA, int indexB)
            {
                return left->Psz()[indexA] == right->Psz()[indexB];
            });
        }
    };

    //-----------------------------------------------------------------------------
    // Function TreeComparer for TreeMatch at function level. View the parse tree as a hierarchy of functions.
    // Ignore statement details.
    //-----------------------------------------------------------------------------
    template <class Allocator>
    class FunctionTreeComparer : public ParseTreeComparer<FunctionTreeComparer<Allocator>, Allocator>
    {
    public:
        FunctionTreeComparer(Allocator* alloc) : ParseTreeComparer(alloc) {}
        FunctionTreeComparer(const FunctionTreeComparer& other) : ParseTreeComparer(other) {}

        // We only have 1 kind of node in this view -- FuncDecl
        int LabelCount() const { return 1; }
        int GetLabel(PNode x) const { return 0; }

        PNode GetParent(PNode x) const
        {
            while (true)
            {
                x = __super::GetParent(x);
                if (!x || x->nop == knopFncDecl || x->nop == knopProg)
                {
                    break;
                }
            }

            return x;
        }

        template <class Func>
        void MapChildren(PNode x, const Func& func) const
        {
            __super::MapChildren(x, [&](PNode child)
            {
                if (child->nop == knopFncDecl)
                {
                    func(child);
                }
                else
                {
                    pThis()->MapChildren(child, func);
                }
            });
        }

        // To compute function node distance, only use their direct child nodes. Do not include descendant nodes
        // under nested child functions.
        template <class Func>
        void MapTreeToComputeDistance(PNode x, const Func& func) const
        {
            func(x);

            __super::MapChildren(x, [&](PNode child)
            {
                if (child->nop == knopFncDecl)
                {
                    func(child); // For child func, output the node itself but don't map its descendants
                }
                else
                {
                    pThis()->MapTreeToComputeDistance(child, func); // recursive into other nodes
                }
            });
        }
    };

    //-----------------------------------------------------------------------------
    // Full TreeComparer for TreeMatch full parse tree. Used for test only.
    //-----------------------------------------------------------------------------
    template <class Allocator>
    class FullTreeComparer : public ParseTreeComparer<FullTreeComparer<Allocator>, Allocator>
    {
    public:
        FullTreeComparer(Allocator* alloc) : ParseTreeComparer(alloc) {}
        FullTreeComparer(const FullTreeComparer& other) : ParseTreeComparer(other) {}
    };

    //-----------------------------------------------------------------------------
    // Visit every node of a parse (sub)tree in preorder. Delegates to Preorder/Postorder of PreorderContext.
    //-----------------------------------------------------------------------------
    template <class PreorderContext>
    void ParseTreePreorder(ParseNode* root, PreorderContext* context)
    {
        class ParseTreePreorderVisitorPolicy : public VisitorPolicyBase<PreorderContext*>
        {
        protected:
            bool Preorder(ParseNode* pnode, Context context) { context->Preorder(pnode); return true; }
            void Postorder(ParseNode* pnode, Context context) { context->Postorder(pnode); }
        };

        ParseNodeVisitor<ParseTreePreorderVisitorPolicy> visitor;
        visitor.Visit(root, context);
    }

    template <class Func>
    void ParseTreePreorder(ParseNode* root, const Func& func)
    {
        class PreorderContext
        {
        private:
            const Func& func;
        public:
            PreorderContext(const Func& func) : func(func) {}
            void Preorder(ParseNode* pnode) { func(pnode); }
            void Postorder(ParseNode* pnode) {}
        };

        PreorderContext context(func);
        ParseTreePreorder(root, &context);
    }

    // TEMP: Consider setting parent at parse time. Temporarily traverse the whole tree to fix parent links.
    template <class Allocator>
    void FixParentLinks(ParseNodePtr root, Allocator* alloc)
    {
        class FixAstParentVisitorContext
        {
        private:
            JsUtil::Stack<ParseNodePtr, Allocator, /*isLeaf*/true> stack;

        public:
            FixAstParentVisitorContext(Allocator* alloc) : stack(alloc) {};

            void Preorder(ParseNode* pnode)
            {
                pnode->parent = !stack.Empty() ? stack.Top() : nullptr;
                stack.Push(pnode);
            }

            void Postorder(ParseNode* pnode)
            {
                Assert(pnode == stack.Peek());
                stack.Pop();
            }
        };

        FixAstParentVisitorContext fixAstParentVisitorContext(alloc);
        ParseTreePreorder(root, &fixAstParentVisitorContext);
    }

    //-----------------------------------------------------------------------------
    // Map child nodes of a parse node.
    //-----------------------------------------------------------------------------
    template <class Func>
    void MapChildren(ParseNode* pnode, const Func& func)
    {
        struct ChildrenWalkerPolicy : public WalkerPolicyBase<bool, const Func&>
        {
            ResultType WalkChildChecked(ParseNode *pnode, Context context)
            {
                // Some of Walker code calls with null ParseNode. e.g., a for loop with null init child.
                if (pnode)
                {
                    context(pnode);
                }
                return true;
            }

            ResultType WalkFirstChild(ParseNode *pnode, Context context) { return WalkChildChecked(pnode, context); }
            ResultType WalkSecondChild(ParseNode *pnode, Context context) { return WalkChildChecked(pnode, context); }
            ResultType WalkNthChild(ParseNode *pparentnode, ParseNode *pnode, Context context) { return WalkChildChecked(pnode, context); }
        };

        ParseNodeWalker<ChildrenWalkerPolicy> walker;
        walker.Walk(pnode, func);
    }

    //-----------------------------------------------------------------------------
    // Helpers for testing ParseNode equivalence
    //-----------------------------------------------------------------------------
    class SyntaxEquivalenceBase
    {
    public:
        //
        // Check if a node is a token node (leaf only, can never have child nodes). e.g., "123" (number literal).
        //
        static bool IsToken(ParseNode* pnode)
        {
            // TODO: We may use a new flag fnopToken
            return (ParseNode::Grfnop(pnode->nop) & fnopLeaf)
                && pnode->nop != knopFncDecl
                && pnode->nop != knopClassDecl;
        }

        //
        // Check if a node has token (node type ownning an implicit token, e.g. "var x" (var declaration)).
        //
        static bool HasToken(ParseNode* pnode)
        {
            // TODO: We may use a new flag fnopHasToken
            return pnode->nop == knopVarDecl
                || pnode->nop == knopFncDecl; // TODO: other nodes with data
        }

        //
        // Check if 2 IsToken nodes (of the same type) are equivalent.
        //
        static bool AreTokensEquivalent(ParseNodePtr left, ParseNodePtr right)
        {
            Assert(IsToken(left) && left->nop == right->nop);

            switch (left->nop)
            {
            case knopName:
            case knopStr:
                return AreEquivalent(left->sxPid.pid, right->sxPid.pid);

            case knopInt:
                return left->sxInt.lw == right->sxInt.lw;

            case knopFlt:
                return left->sxFlt.dbl == right->sxFlt.dbl;

            case knopRegExp:
                //TODO: sxPid.regexPattern
                break;
            }

            // Other tokens have fixed strings and are always equivalent, e.g. "true", "null"
            return true;
        }

        //
        // Check if 2 HasToken nodes (of the same type) have equivalent tokens.
        //
        static bool HaveEquivalentTokens(ParseNodePtr left, ParseNodePtr right)
        {
            Assert(HasToken(left) && left->nop == right->nop);

            switch (left->nop)
            {
            case knopVarDecl:
                return AreEquivalent(left->sxVar.pid, right->sxVar.pid);

            case knopFncDecl:
                return AreEquivalent(left->sxFnc.pid, right->sxFnc.pid);

                //TODO: other nodes with data
            }

            Assert(false);
            return false;
        }

    private:
        // Test if 2 PIDs refer to the same text.
        static bool AreEquivalent(IdentPtr pid1, IdentPtr pid2)
        {
            if (pid1 && pid2)
            {
                // Optimize: If we can have both trees (scanner/parser) share Ident dictionary, this can become pid1 == pid2.
                return pid1->Hash() == pid2->Hash()
                    && pid1->Cch() == pid2->Cch()
                    && wcsncmp(pid1->Psz(), pid2->Psz(), pid1->Cch()) == 0;
            }

            // PIDs may be null, e.g. anonymous function declarations
            return pid1 == pid2;
        }
    };

    template <class Allocator>
    class SyntaxEquivalence : public SyntaxEquivalenceBase
    {
    private:
        // 2 stacks used during equivalence test. (Can mark isLeaf because they don't own the nodes.)
        JsUtil::Stack<ParseNode*, Allocator, /*isLeaf*/true> leftStack, rightStack;

    public:
        SyntaxEquivalence(Allocator* alloc) : leftStack(alloc), rightStack(alloc)
        {}

        //
        // Tests if 2 parse (sub)trees are equivalent.
        //
        bool AreEquivalent(ParseNode* left, ParseNode* right)
        {
            bool result;
            if (TryTestEquivalenceFast(left, right, &result))
            {
                return result;
            }

            Reset(); // Clear possible remaining nodes in leftStack/rightStack
            PushChildren(left, right);

            while (!leftStack.Empty() && leftStack.Count() == rightStack.Count())
            {
                left = leftStack.Pop();
                right = rightStack.Pop();

                if (TryTestEquivalenceFast(left, right, &result))
                {
                    if (!result)
                    {
                        return false;
                    }
                }
                else
                {
                    PushChildren(left, right); // Sub-pair is ok, but need to compare children
                }
            }

            return leftStack.Empty() && rightStack.Empty();
        }

    private:
        void Reset()
        {
            leftStack.Clear();
            rightStack.Clear();
        }

        void PushChildren(ParseNode* left, ParseNode* right)
        {
            Assert(leftStack.Count() == rightStack.Count());
            MapChildren(left, [&](ParseNode* child) { leftStack.Push(child); });
            MapChildren(right, [&](ParseNode* child) { rightStack.Push(child); });
        }

        //
        // Try to test 2 nodes for equivalence. Return true if we can determine the pair equivalence.
        // Otherwise return false, which means the pair test is ok but we need further child nodes comparison.
        //
        static bool TryTestEquivalenceFast(ParseNode* left, ParseNode* right, _Out_ bool* result)
        {
            Assert(left && right);
            if (left == right)
            {
                *result = true; // Same node
                return true;
            }

            if (left->nop != right->nop)
            {
                *result = false; // Different node type
                return true;
            }

            if (IsToken(left))
            {
                *result = AreTokensEquivalent(left, right); // Token comparison suffices
                return true;
            }

            if (HasToken(left) && !HaveEquivalentTokens(left, right))
            {
                *result = false; // Different implicit tokens, e.g. "var x" vs "var y"
                return true;
            }

            return false; // This pair is ok, but not sure about children
        }
    };

} // namespace Js

#endif
