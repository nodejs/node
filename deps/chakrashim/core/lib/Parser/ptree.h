//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct Ident;
typedef Ident *IdentPtr;

class Scope;

/***************************************************************************
Flags for classifying node operators.
***************************************************************************/
const uint fnopNone     = 0x0000;
const uint fnopConst    = 0x0001; // constant
const uint fnopLeaf     = 0x0002; // leaf
const uint fnopUni      = 0x0004; // unary
const uint fnopBin      = 0x0008; // binary
const uint fnopRel      = 0x0010; // relational
const uint fnopAsg      = 0x0020; // assignment
const uint fnopBreak    = 0x0040; // break can be used within this statement
const uint fnopContinue = 0x0080; // continue can be used within this statement
const uint fnopCleanup  = 0x0100; // requires cleanup (eg, with or for-in).
const uint fnopJump     = 0x0200;
const uint fnopNotExprStmt = 0x0400;
const uint fnopBinList  = 0x0800;
const uint fnopExprMask = (fnopLeaf|fnopUni|fnopBin);

/***************************************************************************
Flags for classifying parse nodes.
***************************************************************************/
enum PNodeFlags : ushort
{
    fpnNone                                  = 0x0000,

    // knopFncDecl nodes.
    fpnArguments_overriddenByDecl            = 0x0001, // function has a parameter, let/const decl, class or nested function named 'arguments', which overrides the built-in arguments object
    fpnArguments_varDeclaration              = 0x0002, // function has a var declaration named 'arguments', which may change the way an 'arguments' identifier is resolved

    // knopVarDecl nodes.
    fpnArguments                             = 0x0004,
    fpnHidden                                = 0x0008,

    // Statment nodes.
    fpnExplicitSimicolon                     = 0x0010, // statment terminated by an explicit semicolon
    fpnAutomaticSimicolon                    = 0x0020, // statment terminated by an automatic semicolon
    fpnMissingSimicolon                      = 0x0040, // statment missing terminating semicolon, and is not applicable for automatic semicolon insersion
    fpnDclList                               = 0x0080, // statment is a declaration list
    fpnSyntheticNode                         = 0x0100, // node is added by the parser or does it represent user code
    fpnIndexOperator                         = 0x0200, // dot operator is an optimization of an index operator
    fpnJumbStatement                         = 0x0400, // break or continue that was removed by error recovery

    // Unary/Binary nodes
    fpnCanFlattenConcatExpr                  = 0x0800, // the result of the binary operation can particpate in concat N

    // Potentially overlapping transitor flags
    // These flags are set and cleared during a single node traversal and their values can be used in other node traversals.
    fpnMemberReference                       = 0x1000, // The node is a member reference symbol
    fpnCapturesSyms                          = 0x2000, // The node is a statement (or contains a sub-statement)
                                                       // that captures symbols.
};

/***************************************************************************
Data structs for ParseNodes. ParseNode includes a union of these.
***************************************************************************/
struct PnUni
{
    ParseNodePtr pnode1;
};

struct PnBin
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnode1;
    ParseNodePtr pnode2;
};

struct PnTri
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnode1;
    ParseNodePtr pnode2;
    ParseNodePtr pnode3;
};

struct PnSlot
{
  uint slotIndex;
};

struct PnUniSlot : PnUni
{
  uint slotIndex;
  uint staticFuncId;
};

struct PnInt
{
    long lw;
};

struct PnFlt
{
    double dbl;
    bool maybeInt : 1;
};

class Symbol;
struct PidRefStack;
struct PnPid
{
    IdentPtr pid;
    Symbol **symRef;
    Symbol *sym;
    UnifiedRegex::RegexPattern* regexPattern;
    uint regexPatternIndex;

    void SetSymRef(PidRefStack *ref);
    Symbol **GetSymRef() const { return symRef; }
    Js::PropertyId PropertyIdFromNameNode() const;
};

struct PnVar
{
    ParseNodePtr pnodeNext;
    IdentPtr pid;
    Symbol *sym;
    Symbol **symRef;
    ParseNodePtr pnodeInit;
    BOOLEAN isSwitchStmtDecl;
    BOOLEAN isBlockScopeFncDeclVar;

    void InitDeclNode(IdentPtr name, ParseNodePtr initExpr)
    {
        this->pid = name;
        this->pnodeInit = initExpr;
        this->pnodeNext = nullptr;
        this->sym = nullptr;
        this->symRef = nullptr;
        this->isSwitchStmtDecl = false;
        this->isBlockScopeFncDeclVar = false;
    }
};

struct PnLabel
{
    IdentPtr pid;
    ParseNodePtr pnodeNext;
};

struct PnArrLit : PnUni
{
    uint count;
    uint spreadCount;
    BYTE arrayOfTaggedInts:1;     // indicates that array initialzer nodes are all tagged ints
    BYTE arrayOfInts:1;           // indicates that array initialzer nodes are all ints
    BYTE arrayOfNumbers:1;        // indicates that array initialzer nodes are all numbers
    BYTE hasMissingValues:1;
};

class FuncInfo;

enum PnodeBlockType : unsigned
{
    Global,
    Function,
    Regular,
    Parameter
};

enum FncFlags
{
    kFunctionNone                               = 0,
    kFunctionNested                             = 1 << 0, // True if function is nested in another.
    kFunctionDeclaration                        = 1 << 1, // is this a declaration or an expression?
    kFunctionCallsEval                          = 1 << 2, // function uses eval
    kFunctionUsesArguments                      = 1 << 3, // function uses arguments
    kFunctionHasHeapArguments                   = 1 << 4, // function's "arguments" escape the scope
    kFunctionHasReferencableBuiltInArguments    = 1 << 5, // the built-in 'arguments' object is referenceable in the function
    kFunctionIsAccessor                         = 1 << 6, // function is a property getter or setter
    kFunctionHasNonThisStmt                     = 1 << 7,
    kFunctionStrictMode                         = 1 << 8,
    kFunctionDoesNotEscape                      = 1 << 9, // function is known not to escape its declaring scope
    kFunctionSubsumed                           = 1 << 10, // function expression is a parameter in a call that has no closing paren and should be treated as a global declaration (only occurs during error correction)
    kFunctionHasThisStmt                        = 1 << 11, // function has at least one this.assignment and might be a constructor
    kFunctionHasWithStmt                        = 1 << 12, // function (or child) uses with
    kFunctionIsLambda                           = 1 << 13,
    kFunctionChildCallsEval                     = 1 << 14,
    kFunctionHasDestructuringPattern            = 1 << 15,
    kFunctionHasSuperReference                  = 1 << 16,
    kFunctionIsMethod                           = 1 << 17,
    kFunctionIsClassConstructor                 = 1 << 18, // function is a class constructor
    kFunctionIsBaseClassConstructor             = 1 << 19, // function is a base class constructor
    kFunctionIsClassMember                      = 1 << 20, // function is a class member
    kFunctionNameIsHidden                       = 1 << 21, // True if a named function expression has its name hidden from nested functions
    kFunctionIsGeneratedDefault                 = 1 << 22, // Is the function generated by us as a default (e.g. default class constructor)
    kFunctionHasDefaultArguments                = 1 << 23, // Function has one or more ES6 default arguments
    kFunctionIsStaticMember                     = 1 << 24,
    kFunctionIsGenerator                        = 1 << 25, // Function is an ES6 generator function
    kFunctionAsmjsMode                          = 1 << 26,
    kFunctionHasNewTargetReference              = 1 << 27, // function has a reference to new.target
    kFunctionIsAsync                            = 1 << 28, // function is async
    kFunctionHasDirectSuper                     = 1 << 29, // super()
};

struct RestorePoint;
struct DeferredFunctionStub;

struct PnFnc
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnodeName;
    IdentPtr pid;
    LPCOLESTR hint;
    ulong hintLength;
    ulong hintOffset;
    bool  isNameIdentifierRef;
    ParseNodePtr pnodeScopes;
    ParseNodePtr pnodeBodyScope;
    ParseNodePtr pnodeArgs;
    ParseNodePtr pnodeVars;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeRest;

    FuncInfo *funcInfo; // function information gathered during byte code generation
    Scope *scope;

    uint nestedCount; // Nested function count (valid until children have been processed)
    uint nestedIndex; // Index within the parent function

    uint16 firstDefaultArg; // Position of the first default argument, if any

    unsigned int fncFlags;
    long astSize;
    size_t cbMin; // Min an Lim UTF8 offsets.
    size_t cbLim;
    ULONG lineNumber;   // Line number relative to the current source buffer of the function declaration.
    ULONG columnNumber; // Column number of the declaration.
    Js::LocalFunctionId functionId;
#if DBG
    Js::LocalFunctionId deferredParseNextFunctionId;
#endif
    RestorePoint *pRestorePoint;
    DeferredFunctionStub *deferredStub;

    static const long MaxStackClosureAST = 800000;

private:
    void SetFlags(uint flags, bool set)
    {
        if (set)
        {
            fncFlags |= flags;
        }
        else
        {
            fncFlags &= ~flags;
        }
    }

    bool HasFlags(uint flags) const
    {
        return (fncFlags & flags) == flags;
    }

public:
    void ClearFlags()
    {
        fncFlags = kFunctionNone;
    }

    void SetAsmjsMode(bool set = true) { SetFlags(kFunctionAsmjsMode, set); }
    void SetCallsEval(bool set = true) { SetFlags(kFunctionCallsEval, set); }
    void SetChildCallsEval(bool set = true) { SetFlags(kFunctionChildCallsEval, set); }
    void SetDeclaration(bool set = true) { SetFlags(kFunctionDeclaration, set); }
    void SetDoesNotEscape(bool set = true) { SetFlags(kFunctionDoesNotEscape, set); }
    void SetHasDefaultArguments(bool set = true) { SetFlags(kFunctionHasDefaultArguments, set); }
    void SetHasDestructuringPattern(bool set = true) { SetFlags(kFunctionHasDestructuringPattern, set); }
    void SetHasHeapArguments(bool set = true) { SetFlags(kFunctionHasHeapArguments, set); }
    void SetHasNonThisStmt(bool set = true) { SetFlags(kFunctionHasNonThisStmt, set); }
    void SetHasReferenceableBuiltInArguments(bool set = true) { SetFlags(kFunctionHasReferencableBuiltInArguments, set); }
    void SetHasSuperReference(bool set = true) { SetFlags(kFunctionHasSuperReference, set); }
    void SetHasDirectSuper(bool set = true) { SetFlags(kFunctionHasDirectSuper, set); }
    void SetHasNewTargetReferene(bool set = true) { SetFlags(kFunctionHasNewTargetReference, set); }
    void SetHasThisStmt(bool set = true) { SetFlags(kFunctionHasThisStmt, set); }
    void SetHasWithStmt(bool set = true) { SetFlags(kFunctionHasWithStmt, set); }
    void SetIsAccessor(bool set = true) { SetFlags(kFunctionIsAccessor, set); }
    void SetIsAsync(bool set = true) { SetFlags(kFunctionIsAsync, set); }
    void SetIsClassConstructor(bool set = true) { SetFlags(kFunctionIsClassConstructor, set); }
    void SetIsBaseClassConstructor(bool set = true) { SetFlags(kFunctionIsBaseClassConstructor, set); }
    void SetIsClassMember(bool set = true) { SetFlags(kFunctionIsClassMember, set); }
    void SetIsGeneratedDefault(bool set = true) { SetFlags(kFunctionIsGeneratedDefault, set); }
    void SetIsGenerator(bool set = true) { SetFlags(kFunctionIsGenerator, set); }
    void SetIsLambda(bool set = true) { SetFlags(kFunctionIsLambda, set); }
    void SetIsMethod(bool set = true) { SetFlags(kFunctionIsMethod, set); }
    void SetIsStaticMember(bool set = true) { SetFlags(kFunctionIsStaticMember, set); }
    void SetNameIsHidden(bool set = true) { SetFlags(kFunctionNameIsHidden, set); }
    void SetNested(bool set = true) { SetFlags(kFunctionNested, set); }
    void SetStrictMode(bool set = true) { SetFlags(kFunctionStrictMode, set); }
    void SetSubsumed(bool set = true) { SetFlags(kFunctionSubsumed, set); }
    void SetUsesArguments(bool set = true) { SetFlags(kFunctionUsesArguments, set); }

    bool CallsEval() const { return HasFlags(kFunctionCallsEval); }
    bool ChildCallsEval() const { return HasFlags(kFunctionChildCallsEval); }
    bool DoesNotEscape() const { return HasFlags(kFunctionDoesNotEscape); }
    bool GetArgumentsObjectEscapes() const { return HasFlags(kFunctionHasHeapArguments); }
    bool GetAsmjsMode() const { return HasFlags(kFunctionAsmjsMode); }
    bool GetStrictMode() const { return HasFlags(kFunctionStrictMode); }
    bool HasDefaultArguments() const { return HasFlags(kFunctionHasDefaultArguments); }
    bool HasDestructuringPattern() const { return HasFlags(kFunctionHasDestructuringPattern); }
    bool HasHeapArguments() const { return true; /* HasFlags(kFunctionHasHeapArguments); Disabling stack arguments. Always return HeapArguments as True */ }
    bool HasOnlyThisStmts() const { return !HasFlags(kFunctionHasNonThisStmt); }
    bool HasReferenceableBuiltInArguments() const { return HasFlags(kFunctionHasReferencableBuiltInArguments); }
    bool HasSuperReference() const { return HasFlags(kFunctionHasSuperReference); }
    bool HasDirectSuper() const { return HasFlags(kFunctionHasDirectSuper); }
    bool HasNewTargetReference() const { return HasFlags(kFunctionHasNewTargetReference); }
    bool HasThisStmt() const { return HasFlags(kFunctionHasThisStmt); }
    bool HasWithStmt() const { return HasFlags(kFunctionHasWithStmt); }
    bool IsAccessor() const { return HasFlags(kFunctionIsAccessor); }
    bool IsAsync() const { return HasFlags(kFunctionIsAsync); }
    bool IsClassConstructor() const { return HasFlags(kFunctionIsClassConstructor); }
    bool IsBaseClassConstructor() const { return HasFlags(kFunctionIsBaseClassConstructor); }
    bool IsClassMember() const { return HasFlags(kFunctionIsClassMember); }
    bool IsDeclaration() const { return HasFlags(kFunctionDeclaration); }
    bool IsGeneratedDefault() const { return HasFlags(kFunctionIsGeneratedDefault); }
    bool IsGenerator() const { return HasFlags(kFunctionIsGenerator); }
    bool IsLambda() const { return HasFlags(kFunctionIsLambda); }
    bool IsMethod() const { return HasFlags(kFunctionIsMethod); }
    bool IsNested() const { return HasFlags(kFunctionNested); }
    bool IsStaticMember() const { return HasFlags(kFunctionIsStaticMember); }
    bool IsSubsumed() const { return HasFlags(kFunctionSubsumed); }
    bool NameIsHidden() const { return HasFlags(kFunctionNameIsHidden); }
    bool UsesArguments() const { return HasFlags(kFunctionUsesArguments); }

    bool IsSimpleParameterList() const { return !HasDefaultArguments() && !HasDestructuringPattern() && pnodeRest == nullptr; }

    size_t LengthInBytes()
    {
        return cbLim - cbMin;
    }

    Symbol *GetFuncSymbol();
    void SetFuncSymbol(Symbol *sym);

    ParseNodePtr GetParamScope() const;
    ParseNodePtr *GetParamScopeRef() const;
    ParseNodePtr GetBodyScope() const;
    ParseNodePtr *GetBodyScopeRef() const;
    ParseNodePtr GetTopLevelScope() const
    {
        // Top level scope will be the same for knopProg and knopFncDecl.
        return GetParamScope();
    }

    template<typename Fn>
    void MapContainerScopes(Fn fn)
    {
        fn(this->pnodeScopes->sxBlock.pnodeScopes);
        if (this->pnodeBodyScope != nullptr)
        {
            fn(this->pnodeBodyScope->sxBlock.pnodeScopes);
        }
    }
};

struct PnClass
{
    ParseNodePtr pnodeName;
    ParseNodePtr pnodeDeclName;
    ParseNodePtr pnodeBlock;
    ParseNodePtr pnodeConstructor;
    ParseNodePtr pnodeMembers;
    ParseNodePtr pnodeStaticMembers;
    ParseNodePtr pnodeExtends;
};

struct PnStrTemplate
{
    ParseNodePtr pnodeStringLiterals;
    ParseNodePtr pnodeStringRawLiterals;
    ParseNodePtr pnodeSubstitutionExpressions;
    uint16 countStringLiterals;
    BYTE isTaggedTemplate:1;
};

struct PnProg : PnFnc
{
    ParseNodePtr pnodeLastValStmt;
    bool m_UsesArgumentsAtGlobal;
};

struct PnCall
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnodeTarget;
    ParseNodePtr pnodeArgs;
    uint16 argCount;
    uint16 spreadArgCount;
    BYTE callOfConstants : 1;
    BYTE isApplyCall : 1;
    BYTE isEvalCall : 1;
};

struct PnStmt
{
    ParseNodePtr pnodeOuter;

    // Set by parsing code, used by code gen.
    uint grfnop;

    // Needed for byte code gen.
    Js::ByteCodeLabel breakLabel;
    Js::ByteCodeLabel continueLabel;
};

struct PnBlock : PnStmt
{
    ParseNodePtr pnodeStmt;
    ParseNodePtr pnodeLastValStmt;
    ParseNodePtr pnodeLexVars;
    ParseNodePtr pnodeScopes;
    ParseNodePtr pnodeNext;
    Scope        *scope;

    ParseNodePtr enclosingBlock;
    int blockId;
    PnodeBlockType blockType:2;
    BYTE         callsEval:1;
    BYTE         childCallsEval:1;

    void SetCallsEval(bool does) { callsEval = does; }
    bool GetCallsEval() const { return callsEval; }

    void SetChildCallsEval(bool does) { childCallsEval = does; }
    bool GetChildCallsEval() const { return childCallsEval; }

    void SetEnclosingBlock(ParseNodePtr pnode) { enclosingBlock = pnode; }
    ParseNodePtr GetEnclosingBlock() const { return enclosingBlock; }

    bool HasBlockScopedContent() const;
};

struct PnJump : PnStmt
{
    ParseNodePtr pnodeTarget;
    bool hasExplicitTarget;
};

struct PnLoop : PnStmt
{
    // Needed for byte code gen
    uint loopId;
};

struct PnWhile : PnLoop
{
    ParseNodePtr pnodeCond;
    ParseNodePtr pnodeBody;
};

struct PnWith : PnStmt
{
    ParseNodePtr pnodeObj;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeScopes;
    ParseNodePtr pnodeNext;
    Scope        *scope;
};

struct PnParamPattern
{
    ParseNodePtr pnodeNext;
    Js::RegSlot location;
    ParseNodePtr pnode1;
};

struct PnIf : PnStmt
{
    ParseNodePtr pnodeCond;
    ParseNodePtr pnodeTrue;
    ParseNodePtr pnodeFalse;
};

struct PnHelperCall2 {
  ParseNodePtr pnodeArg1;
  ParseNodePtr pnodeArg2;
  int helperId;
};

struct PnForInOrForOf : PnLoop
{
    ParseNodePtr pnodeObj;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeLval;
    ParseNodePtr pnodeBlock;
    Js::RegSlot itemLocation;
};

struct PnFor : PnLoop
{
    ParseNodePtr pnodeCond;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeInit;
    ParseNodePtr pnodeIncr;
    ParseNodePtr pnodeBlock;
    ParseNodePtr pnodeInverted;
};

struct PnSwitch : PnStmt
{
    ParseNodePtr pnodeVal;
    ParseNodePtr pnodeCases;
    ParseNodePtr pnodeDefault;
    ParseNodePtr pnodeBlock;
};

struct PnCase : PnStmt
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnodeExpr; // nullptr for default
    ParseNodePtr pnodeBody;
    Js::ByteCodeLabel labelCase;
};

struct PnReturn : PnStmt
{
    ParseNodePtr pnodeExpr;
};

struct PnTryFinally : PnStmt
{
    ParseNodePtr pnodeTry;
    ParseNodePtr pnodeFinally;
};

struct PnTryCatch : PnStmt
{
    ParseNodePtr pnodeTry;
    ParseNodePtr pnodeCatch;
};

struct PnTry : PnStmt
{
    ParseNodePtr pnodeBody;
};

struct PnCatch : PnStmt
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnodeParam;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeScopes;
    Scope        *scope;
};

struct PnFinally : PnStmt
{
    ParseNodePtr pnodeBody;
};

struct ParseNode
{
    OpCode nop;
    ushort grfpn;
    charcount_t ichMin;         // start offset into the original source buffer
    charcount_t ichLim;         // end offset into the original source buffer
    Js::RegSlot location;
    bool isUsed;                // indicates whether an expression such as x++ is used
    bool emitLabels;
    bool notEscapedUse;         // Use by byte code generator.  Currently, only used by child of knopComma
    bool isInList;
    bool isCallApplyTargetLoad;
#ifdef EDIT_AND_CONTINUE
    ParseNodePtr parent;
#endif

    union
    {
        PnArrLit        sxArrLit;       // Array literal
        PnBin           sxBin;          // binary operators
        PnBlock         sxBlock;        // block { }
        PnCall          sxCall;         // function call
        PnCase          sxCase;         // switch case
        PnCatch         sxCatch;        // { catch(e : expr) {body} }
        PnClass         sxClass;        // class declaration
        PnFinally       sxFinally;      // finally
        PnFlt           sxFlt;          // double constant
        PnFnc           sxFnc;          // function declaration
        PnFor           sxFor;          // for loop
        PnForInOrForOf  sxForInOrForOf; // for-in loop
        PnHelperCall2   sxHelperCall2;  // call to helper
        PnIf            sxIf;           // if
        PnInt           sxInt;          // integer constant
        PnJump          sxJump;         // break and continue
        PnLabel         sxLabel;        // label nodes
        PnLoop          sxLoop;         // base for loop nodes
        PnPid           sxPid;          // identifier or string
        PnProg          sxProg;         // global program
        PnReturn        sxReturn;       // return [expr]
        PnStmt          sxStmt;         // base for statement nodes
        PnStrTemplate   sxStrTemplate;  // string template declaration
        PnSwitch        sxSwitch;       // switch
        PnTri           sxTri;          // ternary operator
        PnTry           sxTry;          // try-catch
        PnTryCatch      sxTryCatch;     // try-catch
        PnTryFinally    sxTryFinally;   // try-catch-finally
        PnUni           sxUni;          // unary operators
        PnVar           sxVar;          // variable declaration
        PnWhile         sxWhile;        // while and do-while loops
        PnWith          sxWith;         // with
        PnParamPattern  sxParamPattern; // Destructure pattern for function/catch parameter
    };

    IdentPtr name()
    {
        if (this->nop == knopName || this->nop == knopStr)
        {
            return this->sxPid.pid;
        }
        else if (this->nop == knopVarDecl)
        {
            return this->sxVar.pid;
        }
        else if (this->nop == knopConstDecl)
        {
            return this->sxVar.pid;
        }
        return nullptr;
    }

    static const uint mpnopgrfnop[knopLim];

    static uint Grfnop(int nop)
    {
        Assert(nop < knopLim);
        return nop < knopLim ? mpnopgrfnop[nop] : fnopNone;
    }

    BOOL IsStatement()
    {
        return (nop >= knopList && nop != knopLabel) || ((Grfnop(nop) & fnopAsg) != 0);
    }

    uint Grfnop(void)
    {
        Assert(nop < knopLim);
        return nop < knopLim ? mpnopgrfnop[nop] : fnopNone;
    }

    charcount_t LengthInCodepoints() const
    {
        return (this->ichLim - this->ichMin);
    }

    // This node is a function decl node and function has a var declaration named 'arguments',
    bool HasVarArguments() const
    {
        return ((nop == knopFncDecl) && (grfpn & PNodeFlags::fpnArguments_varDeclaration));
    }

    bool CapturesSyms() const
    {
        return (grfpn & PNodeFlags::fpnCapturesSyms) != 0;
    }

    void SetCapturesSyms()
    {
        grfpn |= PNodeFlags::fpnCapturesSyms;
    }

    bool IsInList() const { return this->isInList; }
    void SetIsInList() { this->isInList = true; }

    bool IsNotEscapedUse() const { return this->notEscapedUse; }
    void SetNotEscapedUse() { this->notEscapedUse = true; }

    bool CanFlattenConcatExpr() const { return !!(this->grfpn & PNodeFlags::fpnCanFlattenConcatExpr); }

    bool IsCallApplyTargetLoad() { return isCallApplyTargetLoad; }
    void SetIsCallApplyTargetLoad() { isCallApplyTargetLoad = true; }
    bool IsVarLetOrConst() const
    {
        return this->nop == knopVarDecl || this->nop == knopLetDecl || this->nop == knopConstDecl;
    }

    ParseNodePtr GetFormalNext()
    {
        ParseNodePtr pnodeNext = nullptr;

        if (nop == knopParamPattern)
        {
            pnodeNext = this->sxParamPattern.pnodeNext;
        }
        else
        {
            Assert(IsVarLetOrConst());
            pnodeNext = this->sxVar.pnodeNext;
        }
        return pnodeNext;
    }

    bool IsPattern() const
    {
        return nop == knopObjectPattern || nop == knopArrayPattern;
    }

#if DBG_DUMP
    void Dump();
#endif
};

const int kcbPnNone         = offsetof(ParseNode, sxUni);
const int kcbPnArrLit       = kcbPnNone + sizeof(PnArrLit);
const int kcbPnBin          = kcbPnNone + sizeof(PnBin);
const int kcbPnBlock        = kcbPnNone + sizeof(PnBlock);
const int kcbPnCall         = kcbPnNone + sizeof(PnCall);
const int kcbPnCase         = kcbPnNone + sizeof(PnCase);
const int kcbPnCatch        = kcbPnNone + sizeof(PnCatch);
const int kcbPnClass        = kcbPnNone + sizeof(PnClass);
const int kcbPnFinally      = kcbPnNone + sizeof(PnFinally);
const int kcbPnFlt          = kcbPnNone + sizeof(PnFlt);
const int kcbPnFnc          = kcbPnNone + sizeof(PnFnc);
const int kcbPnFor          = kcbPnNone + sizeof(PnFor);
const int kcbPnForIn        = kcbPnNone + sizeof(PnForInOrForOf);
const int kcbPnForOf        = kcbPnNone + sizeof(PnForInOrForOf);
const int kcbPnHelperCall3  = kcbPnNone + sizeof(PnHelperCall2);
const int kcbPnIf           = kcbPnNone + sizeof(PnIf);
const int kcbPnInt          = kcbPnNone + sizeof(PnInt);
const int kcbPnJump         = kcbPnNone + sizeof(PnJump);
const int kcbPnLabel        = kcbPnNone + sizeof(PnLabel);
const int kcbPnPid          = kcbPnNone + sizeof(PnPid);
const int kcbPnProg         = kcbPnNone + sizeof(PnProg);
const int kcbPnReturn       = kcbPnNone + sizeof(PnReturn);
const int kcbPnSlot         = kcbPnNone + sizeof(PnSlot);
const int kcbPnStrTemplate  = kcbPnNone + sizeof(PnStrTemplate);
const int kcbPnSwitch       = kcbPnNone + sizeof(PnSwitch);
const int kcbPnTri          = kcbPnNone + sizeof(PnTri);
const int kcbPnTry          = kcbPnNone + sizeof(PnTry);
const int kcbPnTryCatch     = kcbPnNone + sizeof(PnTryCatch);
const int kcbPnTryFinally   = kcbPnNone + sizeof(PnTryFinally);
const int kcbPnUni          = kcbPnNone + sizeof(PnUni);
const int kcbPnUniSlot      = kcbPnNone + sizeof(PnUniSlot);
const int kcbPnVar          = kcbPnNone + sizeof(PnVar);
const int kcbPnWhile        = kcbPnNone + sizeof(PnWhile);
const int kcbPnWith         = kcbPnNone + sizeof(PnWith);
const int kcbPnParamPattern = kcbPnNone + sizeof(PnParamPattern);

#define AssertNodeMem(pnode) AssertPvCb(pnode, kcbPnNone)
#define AssertNodeMemN(pnode) AssertPvCbN(pnode, kcbPnNone)
