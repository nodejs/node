declare namespace InternalTraceEventsBinding {
  class CategorySet {
    constructor(categories: unknown[]);
    enable(): void;
    disable(): void;
  }
}

export interface TraceEventsBinding {
  hasAgent(): boolean;
  getEnabledCategories(): string | undefined;
  setTraceCategoryStateUpdateHandler(
    callback: (enabled: boolean) => void,
  ): void;
  getCategoryEnabledBuffer(category: string): Uint8Array | undefined;
  CategorySet: typeof InternalTraceEventsBinding.CategorySet;
  isTraceCategoryEnabled(category: string): boolean;
  trace(
    phase: number,
    category: string,
    name: string,
    id?: number | null,
    data?: unknown,
  ): boolean;
  usePerfetto: boolean;
}
