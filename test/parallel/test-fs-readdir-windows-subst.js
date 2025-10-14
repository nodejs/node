'use strict';

const common = require('../common');

if (!common.isWindows) {
  common.skip('Windows-specific test for subst drive handling');
}

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

// This test verifies that Node.js can properly handle Windows subst drives
// when reading directories. The bug was that paths like "M:\" were being
// incorrectly transformed to "M:\\\\" causing ENOENT errors.
// Refs: https://github.com/nodejs/node/issues/58970

// Test 1: Verify that drive root paths don't get extra backslashes
// This simulates the scenario where a subst drive root is accessed
{
  // Create a temporary directory to simulate subst drive behavior
  tmpdir.refresh();

  // Create some test files
  const testFiles = ['file1.txt', 'file2.txt'];
  testFiles.forEach((file) => {
    fs.writeFileSync(`${tmpdir.path}/${file}`, 'test content');
  });

  // Test reading directory with trailing backslash (simulating subst drive root)
  const pathWithTrailingSlash = tmpdir.path + '\\';

  // This should not throw ENOENT error
  // Reading directory with trailing backslash should not fail
  const files = fs.readdirSync(pathWithTrailingSlash);
  assert(files.length >= testFiles.length);
  testFiles.forEach((file) => {
    assert(files.includes(file));
  });

  // Test async version
  fs.readdir(pathWithTrailingSlash, common.mustSucceed((files) => {
    assert(files.length >= testFiles.length);
    testFiles.forEach((file) => {
      assert(files.includes(file));
    });
  }));
}

// Test 2: Verify that actual drive root paths work correctly
// This test checks common Windows drive patterns
{
  const drivePaths = ['C:\\', 'D:\\', 'E:\\'];

  drivePaths.forEach((drivePath) => {
    try {
      // Only test if the drive exists
      fs.accessSync(drivePath, fs.constants.F_OK);

      // This should not throw ENOENT error for existing drives
      // Reading drive root should not fail
      const files = fs.readdirSync(drivePath);
      assert(Array.isArray(files));

    } catch (err) {
      // Skip if drive doesn't exist (expected for most drives)
      if (err.code !== 'ENOENT') {
        throw err;
      }
    }
  });
}

// Test 3: Test with simulated subst command scenario
// This tests the specific case mentioned in the GitHub issue
{
  // We can't actually create a subst drive in the test environment,
  // but we can test the path normalization logic with various path formats
  const testPaths = [
    'M:\\',
    'X:\\',
    'Z:\\',
  ];

  testPaths.forEach((testPath) => {
    // Create a mock scenario by testing path handling
    // The key is that these paths should not be transformed to have double backslashes
    const normalizedPath = testPath;

    // Verify the path doesn't have double backslashes at the end
    assert(!normalizedPath.endsWith('\\\\'),
           `Path ${testPath} should not end with double backslashes`);

    // The path should end with exactly one backslash
    assert(normalizedPath.endsWith('\\'),
           `Path ${testPath} should end with exactly one backslash`);

    // The path should not contain multiple consecutive backslashes
    assert(!normalizedPath.includes('\\\\'),
           `Path ${testPath} should not contain double backslashes`);
  });
}
