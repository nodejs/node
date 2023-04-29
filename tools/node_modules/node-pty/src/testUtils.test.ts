/**
 * Copyright (c) 2019, Microsoft Corporation (MIT License).
 */

export function pollUntil(cb: () => boolean, timeout: number, interval: number): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    const intervalId = setInterval(() => {
      if (cb()) {
        clearInterval(intervalId);
        clearTimeout(timeoutId);
        resolve();
      }
    }, interval);
    const timeoutId = setTimeout(() => {
      clearInterval(intervalId);
      if (cb()) {
        resolve();
      } else {
        reject();
      }
    }, timeout);
  });
}
