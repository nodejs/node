// Compares a list of performance entries to a predefined one.
// actualEntries is an array of performance entries from the user agent,
// and expectedEntries is an array of performance entries minted by the test.
// The comparison doesn't assert the order of the entries.
function checkEntries(actualEntries, expectedEntries) {
  assert_equals(actualEntries.length, expectedEntries.length,
      `The length of actual and expected entries should match.
      actual: ${JSON.stringify(actualEntries)},
      expected: ${JSON.stringify(expectedEntries)}`);
  const actualEntrySet = new Set(actualEntries.map(ae=>ae.name));
  assert_equals(actualEntrySet.size, actualEntries.length, `Actual entry names are not unique: ${JSON.stringify(actualEntries)}`);
  const expectedEntrySet = new Set(expectedEntries.map(ee=>ee.name));
  assert_equals(expectedEntrySet.size, expectedEntries.length, `Expected entry names are not unique: ${JSON.stringify(expectedEntries)}`);
  actualEntries.forEach(ae=>{
    const expectedEntry = expectedEntries.find(e=>e.name === ae.name);
    assert_true(!!expectedEntry, `Entry name '${ae.name}' was not found.`);
    checkEntry(ae, expectedEntry);
  });
}

function checkEntry(entry, {name, entryType, startTime, detail, duration}) {
  assert_equals(entry.name, name);
  assert_equals(entry.entryType, entryType);
  if (startTime !== undefined)
    assert_equals(entry.startTime, startTime);
  if (detail !== undefined)
    assert_equals(JSON.stringify(entry.detail), JSON.stringify(detail));
  if (duration !== undefined)
    assert_equals(entry.duration, duration);
}
