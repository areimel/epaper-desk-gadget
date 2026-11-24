// Lets `node --test device-ui/tests` work: Node 24 resolves a directory
// argument to its index.js, so this file loads every *.test.mjs beside it.
import { readdirSync } from 'node:fs';

const dir = new URL('./', import.meta.url);
for (const name of readdirSync(dir).filter((n) => n.endsWith('.test.mjs')).sort()) {
  await import(new URL(name, dir));
}
