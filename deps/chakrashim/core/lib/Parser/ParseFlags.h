//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


// Parse flags
enum
{
    fscrNil = 0,
    fscrHtmlComments = 1 << 0,   // throw away html style comments
    fscrReturnExpression = 1 << 1,   // call should return the last expression
    fscrImplicitThis = 1 << 2,   // 'this.' is optional (for Call)
    fscrImplicitParents = 1 << 3,   // the parents of 'this' are implicit
    fscrMapQuote = 1 << 4,   // map single quote to double quote
    fscrDynamicCode = 1 << 5,   // The code is being generated dynamically (eval, new Function, etc.)
    fscrSyntaxColor = 1 << 6,   // used by the scanner for syntax coloring
    fscrNoImplicitHandlers = 1 << 7,   // same as Opt NoConnect at start of block

                                       // prevents the need to make a copy to strip off trailing html comments
                                       // - modifies the behavior of fscrHtmlComments
    fscrDoNotHandleTrailingHtmlComments = 1 << 8,

#if DEBUG
    fscrEnforceJSON = 1 << 9,  // used together with fscrReturnExpression
                               // enforces JSON semantics in the parsing.
#endif

    fscrEval = 1 << 10,  // this expression has eval semantics (i.e., run in caller's context
    fscrEvalCode = 1 << 11,  // this is an eval expression
    fscrGlobalCode = 1 << 12,  // this is a global script
    fscrDeferFncParse = 1 << 13,  // parser: defer creation of AST's for non-global code
    fscrDeferredFncExpression = 1 << 14,  // the function decl node we deferred is an expression,
                                          // i.e., not a declaration statement
    fscrDeferredFnc = 1 << 15,  // the function we are parsing is deferred
    fscrNoPreJit = 1 << 16,  // ignore prejit global flag
    fscrAllowFunctionProxy = 1 << 17,  // Allow creation of function proxies instead of function bodies
    fscrIsLibraryCode = 1 << 18,  // Current code is engine library code written in Javascript
    fscrNoDeferParse = 1 << 19,  // Do not defer parsing
    // Unused = 1 << 20,
#ifdef IR_VIEWER
    fscrIrDumpEnable = 1 << 21,  // Allow parseIR to generate an IR dump
#endif /* IRVIEWER */

                                 // Throw a ReferenceError when the global 'this' is used (possibly in a lambda),
                                 // for debugger when broken in a lambda that doesn't capture 'this'
    fscrDebuggerErrorOnGlobalThis = 1 << 22,
    fscrDeferredClassMemberFnc = 1 << 23,
    fscrConsoleScopeEval = 1 << 24,  //  The eval string is console eval or debugEval, used to have top level
                                     //  let/const in global scope instead of eval scope so that they can be preserved across console inputs
    fscrNoAsmJs = 1 << 25, // Disable generation of asm.js code
    fscrAll = (1 << 26) - 1
};

