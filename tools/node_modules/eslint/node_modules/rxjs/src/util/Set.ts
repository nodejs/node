import { root } from './root';

export interface ISetCtor {
  new<T>(): ISet<T>;
}

export interface ISet<T> {
  add(value: T): void;
  has(value: T): boolean;
  size: number;
  clear(): void;
}

export function minimalSetImpl<T>(): ISetCtor {
  // THIS IS NOT a full impl of Set, this is just the minimum
  // bits of functionality we need for this library.
  return class MinimalSet<T> implements ISet<T> {
    private _values: T[] = [];

    add(value: T): void {
      if (!this.has(value)) {
        this._values.push(value);
      }
    }

    has(value: T): boolean {
      return this._values.indexOf(value) !== -1;
    }

    get size(): number {
      return this._values.length;
    }

    clear(): void {
      this._values.length = 0;
    }
  };
}

export const Set: ISetCtor = root.Set || minimalSetImpl();