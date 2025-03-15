import * as fs from 'fs';

// Path for temporary file to track execution
const setupFlagPath = process.env.SETUP_FLAG_PATH;
const teardownFlagPath = process.env.TEARDOWN_FLAG_PATH;

interface SetupContext {
  context: {
    setupData?: string;
    [key: string]: any;
  };
}

async function globalSetup({ context }: SetupContext): Promise<void> {
  console.log('Global setup executed');
  context.setupData = 'data from setup';
  if (setupFlagPath) {
    fs.writeFileSync(setupFlagPath, 'Setup was executed');
  }
}

async function globalTeardown({ context }: SetupContext): Promise<void> {
  console.log('Global teardown executed');
  console.log(`Data from setup: ${context.setupData}`);
  if (teardownFlagPath) {
    fs.writeFileSync(teardownFlagPath, 'Teardown was executed');
  }
  
  // Clean up the setup file too
  if (setupFlagPath && fs.existsSync(setupFlagPath)) {
    fs.unlinkSync(setupFlagPath);
  }
}

export { globalSetup, globalTeardown };
