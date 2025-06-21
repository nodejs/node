import * as fs from 'node:fs';

// Path for temporary file to track execution
const setupFlagPath = process.env.SETUP_FLAG_PATH;
const teardownFlagPath = process.env.TEARDOWN_FLAG_PATH;

async function globalSetup(): Promise<void> {
  console.log('Global setup executed');
  if (setupFlagPath) {
    fs.writeFileSync(setupFlagPath, 'Setup was executed');
  }
}

async function globalTeardown(): Promise<void> {
  console.log('Global teardown executed');
  if (teardownFlagPath) {
    fs.writeFileSync(teardownFlagPath, 'Teardown was executed');
  }
  if (setupFlagPath) {
    fs.rmSync(setupFlagPath, { force: true });
  }
}

export { globalSetup, globalTeardown };
