export interface CodeIntegrityBinding {
  isAllowedToExecuteFile(filePath: string) : boolean;
  isFileTrustedBySystemCodeIntegrityPolicy(filePath: string) : boolean;
  isInteractiveModeDisabled() : boolean;
  isSystemEnforcingCodeIntegrity() : boolean;
}
