// test-fix.js
const { spawn } = require('child_process');
const isWindows = process.platform === 'win32';

if (isWindows) {
  // Test the actual fix on Windows
  const cmd = spawn('cmd.exe', ['/c', 'echo 中文测试'], { stdio: 'pipe' });
  cmd.stdout.on('data', (data) => console.log('Windows Output:', data.toString()));
  cmd.on('exit', () => console.log('Process exited'));
} else {
  console.log('This test requires Windows. Skipping cmd.exe tests on macOS.');
  // Optional: Simulate expected behavior
  console.log('Simulated Output (Windows would show): 中文测试');
}
