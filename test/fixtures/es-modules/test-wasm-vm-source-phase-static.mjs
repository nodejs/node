// Test fixture that throws for vm source phase static import
import vm from 'node:vm';

const m1 = new vm.SourceTextModule('import source x from "y";');
const m2 = new vm.SourceTextModule('export var p = 5;');
await m1.link(() => m2);
await m1.evaluate();
