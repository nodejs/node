// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } =
    InspectorTest.start('console.table');

(async function test() {
  Protocol.Runtime.enable();
  Protocol.Runtime.evaluate({
    expression: `console.table(['apples', 'oranges', 'bananas'])`
  });
  await waitConsoleAPICalledAndDump();
  Protocol.Runtime.evaluate({
    expression: `function Person(firstName, lastName) {
      this.firstName = firstName;
      this.lastName = lastName;
    };
    var me = new Person('John', 'Smith');
    console.table(me);`
  });
  await waitConsoleAPICalledAndDump();
  Protocol.Runtime.evaluate({
    expression: `var people = [
        ['John', 'Smith'], ['Jane', 'Doe'], ['Emily', 'Jones']];
      console.table(people);`
  });
  await waitConsoleAPICalledAndDump();
  Protocol.Runtime.evaluate({
    expression: `function Person(firstName, lastName) {
        this.firstName = firstName;
        this.lastName = lastName;
      }

      var john = new Person('John', 'Smith');
      var jane = new Person('Jane', 'Doe');
      var emily = new Person('Emily', 'Jones');

      console.table([john, jane, emily]);`
  });
  await waitConsoleAPICalledAndDump();
  Protocol.Runtime.evaluate({
    expression: `var family = {};

      family.mother = new Person('Jane', 'Smith');
      family.father = new Person('John', 'Smith');
      family.daughter = new Person('Emily', 'Smith');

      console.table(family);`
  });
  await waitConsoleAPICalledAndDump();
  Protocol.Runtime.evaluate({
    expression: `console.table([john, jane, emily], ['firstName'])`
  });
  await waitConsoleAPICalledAndDump();
  InspectorTest.completeTest();
})()

async function waitConsoleAPICalledAndDump() {
  const { params : {
    args: [ arg ]
  } } = await Protocol.Runtime.onceConsoleAPICalled();
  InspectorTest.logMessage(arg.preview);
}
