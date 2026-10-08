// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --verify-heap

function Node(next) {
    this.next = next;
}

function Foo(weakMap, barList) {
    this.weakMap = weakMap;
    this.barList = barList;
}

function Bar() {}

function createLinkedList(tail, length) {
    let head = tail;
    for (let i = 0; i < length; ++i) {
        head = new Node(head);
    }
    return head;
}

function getListTail(head) {
    while (head instanceof Node) {
        head = head.next;
    }
    return head;
}

function createChain() {
    let value = 42;

    // Build a chain where each Foo stores a WeakMap and a Bar object
    // hidden behind a singly-linked list. The Bar is a key in the WeakMap,
    // keeping the next Foo alive. By hiding Bar behind the linked list,
    // the marker is likely to encounter the WeakMap before discovering
    // that Bar is reachable, triggering the linear-time algorithm for
    // ephemeron processing.
    for (let i = 0; i < 200; ++i) {
        let weakMap = new WeakMap();
        let bar = new Bar();
        weakMap.set(bar, value);
        let barList = createLinkedList(bar, 200);
        value = new Foo(weakMap, barList);
    }

    return value;
}

let root = createChain();
gc();

let current = root;
while (current instanceof Foo) {
    let bar = getListTail(current.barList);
    current = current.weakMap.get(bar);
}
assertEquals(42, current);
