import { writeFileSync } from 'node:fs';

let counter = 0;

export async function initialize() {
  writeFileSync(1, `hooks initialize ${++counter}\n`);
}
