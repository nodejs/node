process.mixin(require("./common"));

assertEquals(14, process._byteLength("Il était tué"));
assertEquals(14, process._byteLength("Il était tué", "utf8"));

assertEquals(12, process._byteLength("Il était tué", "ascii"));

assertEquals(12, process._byteLength("Il était tué", "binary"));

assertThrows('process._byteLength()');
assertThrows('process._byteLength(5)');