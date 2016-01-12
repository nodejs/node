//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Using super is invalid in many places.
// These tests are generated and are intended to use to verify the restriction is done properly in various contexts
// Here is the C# code used the generate these test cases

/*
namespace ManagedWorkspace
{
    using System;
    using System.Collections.Generic;

    // state = 0 -> not allowed
    // state = 1 -> all allowed
    // state = 2 -> only member allowed
    internal static class Program
    {
        private static void Main(string[] args)
        {
            LeafNode n = new LeafNode();
            BranchNode rootNode = new BranchNode();
            BranchNode childNode = new BranchNode();
            BranchOut(rootNode, n);
            BranchOut(rootNode, childNode);
            BranchOut(childNode, n);

            int testCaseNumber = 1;
            foreach (var code in rootNode.Generate(0))
            {
                Console.WriteLine(GenerateTest(code, testCaseNumber));
                testCaseNumber++;
            }
        }

        private static string GenerateTest(Tuple<string, bool> testCase, int testCaseNumber)
        {
            string goodFormatString = @"assert.doesNotThrow(function () {{ WScript.LoadScript("","samethread").WScript.LoadScript('{0}'); }}, ""generated test #{1}"");";
            string badFormatString = @"assert.throws(() => {{ WScript.LoadScript("","samethread").WScript.LoadScript('{0}'); }}, SyntaxError, ""generatedTest #{1}"", ""Invalid use of the 'super' keyword"");";
            if (testCase.Item2)
            {
                return string.Format(goodFormatString, testCase.Item1, testCaseNumber);
            }
            else
            {
                return string.Format(badFormatString, testCase.Item1, testCaseNumber);
            }
        }

        private static void BranchOut(BranchNode rootNode, Node n)
        {
            rootNode.branches.Add(new Branch("class X { constructor(){+} }", n, (oldState) => 1));
            rootNode.branches.Add(new Branch("class X { x(){+} }", n, (oldState) => 2));
            rootNode.branches.Add(new Branch("function x(){+}", n, (oldState) => 0));
            rootNode.branches.Add(new Branch("() => {+}", n, (oldState) => oldState));
        }
    }

    class Branch
    {
        public Branch(string userFormatString, Node branchTarget, Func<int, int> statePropagator)
        {
            this.BranchTarget = branchTarget;
            this.FormatString = ToFormatString(userFormatString);
            this.StatePropagator = statePropagator;
        }

        public Node BranchTarget { get; set; }

        public string FormatString { get; set; }

        public Func<int, int> StatePropagator { get; set; }

        private string ToFormatString(string p)
        {
            return p.Replace("{", "{{").Replace("}", "}}").Replace("+", "{0}");
        }
    }

    abstract class Node
    {
        public abstract IEnumerable<Tuple<string, bool>> Generate(int state);
    }

    class LeafNode : Node
    {
        public override IEnumerable<Tuple<string, bool>> Generate(int state)
        {
            yield return Tuple.Create("super();", state == 1);
            yield return Tuple.Create("super.x;", state > 0);
        }
    }

    class BranchNode : Node
    {
        public List<Branch> branches = new List<Branch>();

        public override IEnumerable<Tuple<string, bool>> Generate(int state)
        {
            foreach (var branch in branches)
            {
                Node branchTarget = branch.BranchTarget;
                string branchFormatString = branch.FormatString;
                foreach (var childItem in branchTarget.Generate(branch.StatePropagator(state)))
                {
                    string childString = childItem.Item1;
                    bool childResult = childItem.Item2;
                    yield return Tuple.Create(string.Format(branchFormatString, childString), childResult);
                }
            }
        }
    }
}
 */

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Generated tests",
        body: function () {

            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){super();} }'); }, "generated test #1");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){super.x;} }'); }, "generated test #2");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){super();} }'); }, SyntaxError, "generatedTest #3", "Invalid use of the 'super' keyword: generatedTest #3");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){super.x;} }'); }, "generated test #4");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){super();}'); }, SyntaxError, "generatedTest #5", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){super.x;}'); }, SyntaxError, "generatedTest #6", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('() => {super();}'); }, SyntaxError, "generatedTest #7", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('() => {super.x;}'); }, SyntaxError, "generatedTest #8", "Invalid use of the 'super' keyword");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){class X { constructor(){super();} }} }'); }, "generated test #9");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){class X { constructor(){super.x;} }} }'); }, "generated test #10");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){class X { x(){super();} }} }'); }, SyntaxError, "generatedTest #11", "Invalid use of the 'super' keyword");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){class X { x(){super.x;} }} }'); }, "generated test #12");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){function x(){super();}} }'); }, SyntaxError, "generatedTest #13", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){function x(){super.x;}} }'); }, SyntaxError, "generatedTest #14", "Invalid use of the 'super' keyword");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){() => {super();}} }'); }, "generated test #15");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { constructor(){() => {super.x;}} }'); }, "generated test #16");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){class X { constructor(){super();} }} }'); }, "generated test #17");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){class X { constructor(){super.x;} }} }'); }, "generated test #18");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){class X { x(){super();} }} }'); }, SyntaxError, "generatedTest #19", "Invalid use of the 'super' keyword");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){class X { x(){super.x;} }} }'); }, "generated test #20");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){function x(){super();}} }'); }, SyntaxError, "generatedTest #21", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){function x(){super.x;}} }'); }, SyntaxError, "generatedTest #22", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){() => {super();}} }'); }, SyntaxError, "generatedTest #23", "Invalid use of the 'super' keyword");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('class X { x(){() => {super.x;}} }'); }, "generated test #24");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){class X { constructor(){super();} }}'); }, "generated test #25");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){class X { constructor(){super.x;} }}'); }, "generated test #26");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){class X { x(){super();} }}'); }, SyntaxError, "generatedTest #27", "Invalid use of the 'super' keyword");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){class X { x(){super.x;} }}'); }, "generated test #28");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){function x(){super();}}'); }, SyntaxError, "generatedTest #29", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){function x(){super.x;}}'); }, SyntaxError, "generatedTest #30", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){() => {super();}}'); }, SyntaxError, "generatedTest #31", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('function x(){() => {super.x;}}'); }, SyntaxError, "generatedTest #32", "Invalid use of the 'super' keyword");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('() => {class X { constructor(){super();} }}'); }, "generated test #33");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('() => {class X { constructor(){super.x;} }}'); }, "generated test #34");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('() => {class X { x(){super();} }}'); }, SyntaxError, "generatedTest #35", "Invalid use of the 'super' keyword");
            assert.doesNotThrow(function () { WScript.LoadScript("","samethread").WScript.LoadScript('() => {class X { x(){super.x;} }}'); }, "generated test #36");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('() => {function x(){super();}}'); }, SyntaxError, "generatedTest #37", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('() => {function x(){super.x;}}'); }, SyntaxError, "generatedTest #38", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('() => {() => {super();}}'); }, SyntaxError, "generatedTest #39", "Invalid use of the 'super' keyword");
            assert.throws(() => { WScript.LoadScript("","samethread").WScript.LoadScript('() => {() => {super.x;}}'); }, SyntaxError, "generatedTest #40", "Invalid use of the 'super' keyword");
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
