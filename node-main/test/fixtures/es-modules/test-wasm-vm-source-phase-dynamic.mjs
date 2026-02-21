// Test fixture that throws for vm source phase dynamic import
import vm from 'node:vm';

const opts = { importModuleDynamically: () => m2 };
const m1 = new vm.SourceTextModule('await import.source("y");', opts);
const m2 = new vm.SourceTextModule('export var p = 5;');
await m1.link(() => m2);
await m1.evaluate();
