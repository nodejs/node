'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');

// Specify the path for the test file
const filePath = path.join(__dirname, '速.txt');

// Create a file with the specified name
fs.writeFileSync(filePath, 'This is a test file containing special characters in the name.');

// Function to try removing the file and verify the outcome
function testRemoveFile() {
  try {
    // Attempt to remove the file
    fs.rmSync(filePath);
    
    // Check if the file still exists to confirm it was removed
    assert.ok(!fs.existsSync(filePath), 'The file should be removed.');

    console.log('File removed successfully.');
  } catch (error) {
    // Output an error if the removal fails
    console.error('Failed to remove file:', error);
    
    // Explicitly fail the test if an error is thrown
    assert.fail('File removal should not throw an error.');
  }
}

// Run the test function
testRemoveFile();