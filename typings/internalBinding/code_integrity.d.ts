export interface CodeIntegrityBinding {
  isFileTrustedBySystemCodeIntegrityPolicy(fd: number) : boolean;
  isInteractiveModeDisabled() : boolean;
  isSystemEnforcingCodeIntegrity() : boolean;
}
