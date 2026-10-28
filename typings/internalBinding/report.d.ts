export interface ReportBinding {
  // Returns the filename, or an empty string if the file cannot be opened.
  writeReport(
    message: string,
    trigger: string,
    filename: string | null | undefined,
    error: unknown,
  ): string;
  // Serialized JSON; the public process.report wrapper parses this string.
  getReport(error: unknown): string;
  getCompact(): boolean;
  setCompact(value: boolean): void;
  getExcludeNetwork(): boolean;
  setExcludeNetwork(value: boolean): void;
  getExcludeEnv(): boolean;
  setExcludeEnv(value: boolean): void;
  getDirectory(): string;
  setDirectory(value: string): void;
  getFilename(): string;
  setFilename(value: string): void;
  getSignal(): string;
  setSignal(value: string): void;
  shouldReportOnFatalError(): boolean;
  setReportOnFatalError(value: boolean): void;
  shouldReportOnSignal(): boolean;
  setReportOnSignal(value: boolean): void;
  shouldReportOnUncaughtException(): boolean;
  setReportOnUncaughtException(value: boolean): void;
}
