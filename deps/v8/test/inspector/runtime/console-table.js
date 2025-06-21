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
  Protocol.Runtime.evaluate({
    expression: `var bigTable = new Array(999);
      bigTable.fill(['a', 'b']);
      console.table(bigTable);`
  });
  await waitConsoleAPICalledAndDump(true /* concise */);
  Protocol.Runtime.evaluate({
    expression: `var bigTable = new Array(1001);
      bigTable.fill(['a', 'b']);
      console.table(bigTable);`
  });
  await waitConsoleAPICalledAndDump(true /* concise */);
  Protocol.Runtime.evaluate({
    expression: `var table = [{a:1, b:2, c:3}, {c:3}];
      var filter = ['c', 'b'];
      console.table(table, filter);`
  });
  await waitConsoleAPICalledAndDump();
  Protocol.Runtime.evaluate({
    expression: `var table = [{a:1, b:2, c:3}, {c:3}];
      var filter = ['c', 'b', 'c'];
      console.table(table, filter);`
  });
  await waitConsoleAPICalledAndDump();
  InspectorTest.completeTest();
})()

/**
 * @param {boolean=} concise
 */
async function waitConsoleAPICalledAndDump(concise) {
  const { params : {
    args: [ arg ]
  } } = await Protocol.Runtime.onceConsoleAPICalled();
  const preview = arg.preview;
  if (concise)
    simplifyAndPrintLast(preview);
  else
    InspectorTest.logMessage(arg.preview);

  function simplifyAndPrintLast(preview) {
    if (!Array.isArray(preview.properties))
      return;
    const properties = preview.properties;
    const lastProperty = properties[properties.length - 1];
    if (lastProperty.valuePreview && lastProperty.valuePreview.properties) {
      const innerProperties = lastProperty.valuePreview.properties;
      InspectorTest.logMessage(`last value property:`);
      InspectorTest.logMessage(innerProperties[innerProperties.length - 1]);
      lastProperty.valuePreview.properties = `<ValuePreviewPropertiesArray(${innerProperties.length})>`;
    }
    InspectorTest.logMessage(`last property:`);
    InspectorTest.logMessage(lastProperty);
    preview.properties = `<PropertiesArray(${properties.length})>`;
    InspectorTest.logMessage(`preview:`);
    InspectorTest.logMessage(preview);
    InspectorTest.logMessage(``);
  }
}
