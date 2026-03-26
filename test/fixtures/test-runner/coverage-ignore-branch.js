// Test fixture for issue #61586
// Tests that node:coverage ignore next also excludes BRDA entries

function getValue(condition) {
  if (condition) {
    return 'truthy';
  }
  /* node:coverage ignore next */
  return 'falsy';
}

// Call only the truthy branch
getValue(true);
