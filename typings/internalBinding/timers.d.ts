export interface TimersBinding {
  getLibuvNow(): number;
  setupTimers(immediateCallback: () => void, timersCallback: (now: number) => void): void;
  scheduleTimer(msecs: number): void;
  toggleTimerRef(value: boolean): void;
  toggleImmediateRef(value: boolean): void;
  immediateInfo: Uint32Array;
}
