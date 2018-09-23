### Esrecurse [![Build Status](https://secure.travis-ci.org/estools/esrecurse.png)](http://travis-ci.org/estools/esrecurse)

Esrecurse ([esrecurse](http://github.com/estools/esrecurse)) is
[ECMAScript](http://www.ecma-international.org/publications/standards/Ecma-262.htm)
recursive traversing functionality.

### Example Usage

The following code will output all variables declared at the root of a file.

```javascript
esrecurse.visit(ast, {
    XXXStatement: function (node) {
        this.visit(node.left);
        // do something...
        this.visit(node.right);
    }
});
```

We can use `Visitor` instance.

```javascript
var visitor = new esrecurse.Visitor({
    XXXStatement: function (node) {
        this.visit(node.left);
        // do something...
        this.visit(node.right);
    }
});

visitor.visit(ast);
```

We can inherit `Visitor` instance easily.

```javascript
function DerivedVisitor() {
    esrecurse.Visitor.call(/* this for constructor */  this  /* visitor object automatically becomes this. */);
}
util.inherits(DerivedVisitor, esrecurse.Visitor);
DerivedVisitor.prototype.XXXStatement = function (node) {
    this.visit(node.left);
    // do something...
    this.visit(node.right);
};
```

And you can invoke default visiting operation inside custom visit operation.

```javascript
function DerivedVisitor() {
    esrecurse.Visitor.call(/* this for constructor */  this  /* visitor object automatically becomes this. */);
}
util.inherits(DerivedVisitor, esrecurse.Visitor);
DerivedVisitor.prototype.XXXStatement = function (node) {
    // do something...
    this.visitChildren(node);
};
```

### License

Copyright (C) 2014 [Yusuke Suzuki](http://github.com/Constellation)
 (twitter: [@Constellation](http://twitter.com/Constellation)) and other contributors.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
