// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


new BenchmarkSuite('Generators', [1000], [
  new Benchmark('Basic', false, false, 0, Basic),
  new Benchmark('Loop', false, false, 0, Loop),
  new Benchmark('Input', false, false, 0, Input),
  new Benchmark('YieldStar', false, false, 0, YieldStar),
]);


// ----------------------------------------------------------------------------
// Benchmark: Basic
// ----------------------------------------------------------------------------

function* five() {
  yield 1;
  yield 2;
  yield 3;
  yield 4;
  yield 5;
}

function Basic() {
  let g = five();
  let sum = 0;
  sum += g.next().value;
  sum += g.next().value;
  sum += g.next().value;
  sum += g.next().value;
  sum += g.next().value;
  if (sum != 15 || !g.next().done) throw "wrong";
}


// ----------------------------------------------------------------------------
// Benchmark: Loop
// ----------------------------------------------------------------------------

function* fibonacci() {
  let x = 0;
  let y = 1;
  yield x;
  while (true) {
    yield y;
    let tmp = x;
    x = y;
    y += tmp;
  }
}

function Loop() {
  let n = 0;
  let x;
  for (x of fibonacci()) {
    if (++n === 42) break;
  }
  if (x != 165580141) throw "wrong";
}



// ----------------------------------------------------------------------------
// Benchmark: Input
// ----------------------------------------------------------------------------

function* multiples(x) {
  let skip = 2;
  let next = 0;
  while (true) {
    if (skip === 0) {
      skip = yield next;
    } else {
      skip--;
    }
    next += x;
  }
}

function Input() {
  let g = multiples(3);
  results = [g.next(2), g.next(0), g.next(5), g.next(10)];
  if (results.slice(-1)[0].value != 60) throw "wrong";
}


// ----------------------------------------------------------------------------
// Benchmark: YieldStar
// ----------------------------------------------------------------------------

function* infix(node) {
  if (node) {
    yield* infix(node.left);
    yield node.label;
    yield* infix(node.right);
  }
}

class Node {
  constructor(label, left, right) {
    this.label = label;
    this.left = left;
    this.right = right;
  }
}

function YieldStar() {
  let tree = new Node(1,
      new Node(2,
          new Node(3,
              new Node(4,
                  new Node(16,
                      new Node(5,
                          new Node(23,
                              new Node(0),
                              new Node(17)),
                          new Node(44, new Node(20)))),
                  new Node(7,
                      undefined,
                      new Node(23,
                          new Node(0),
                          new Node(41, undefined, new Node(11))))),
              new Node(8)),
          new Node(5)),
      new Node(6, undefined, new Node(7)));
  let labels = [...(infix(tree))];
  // 0,23,17,5,20,44,16,4,7,0,23,41,11,3,8,2,5,1,6,7
  if (labels[0] != 0) throw "wrong";
}
