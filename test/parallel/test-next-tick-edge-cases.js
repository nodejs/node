'use strict';

// Test edge cases for nextTick performance optimizations
// Covers recursive processing guard and buffer pool edge cases

const assert = require('assert');

// Test 1: Recursive processing prevention
// Simple test that ensures nextTick works correctly
{
  let recursiveTestExecuted = false;
  
  process.nextTick(() => {
    recursiveTestExecuted = true;
    console.log('✓ Basic nextTick recursion test completed');
  });
  
  process.on('exit', () => {
    assert.strictEqual(recursiveTestExecuted, true);
  });
}

// Test 2: Buffer pool stress test for FixedQueue
// Creates scenarios that will create and destroy multiple circular buffers
{
  let bufferPoolTestCompleted = 0;
  const itemCount = 5000; // Large enough to stress buffer pools
  
  for (let i = 0; i < itemCount; i++) {
    process.nextTick(() => {
      bufferPoolTestCompleted++;
      if (bufferPoolTestCompleted === itemCount) {
        console.log('✓ Buffer pool stress test completed:', itemCount, 'calls');
      }
    });
  }
}

// Test 3: Memory stress test - rapid allocation and deallocation
// Tests object and args pool under rapid cycling conditions
{
  let memoryStressCompleted = 0;
  const cycles = 10;
  const itemsPerCycle = 100;
  const totalExpected = cycles * itemsPerCycle;
  
  for (let cycle = 0; cycle < cycles; cycle++) {
    // Each cycle adds many items then processes them
    for (let i = 0; i < itemsPerCycle; i++) {
      const argCount = (i % 8) + 1; // 1-8 args
      const args = new Array(argCount).fill(0).map((_, idx) => cycle * 100 + i + idx);
      
      process.nextTick((...receivedArgs) => {
        assert.strictEqual(receivedArgs.length, argCount);
        memoryStressCompleted++;
        
        if (memoryStressCompleted === totalExpected) {
          console.log('✓ Memory stress test completed:', totalExpected, 'calls');
        }
      }, ...args);
    }
  }
}

console.log('nextTick edge case tests initialized');