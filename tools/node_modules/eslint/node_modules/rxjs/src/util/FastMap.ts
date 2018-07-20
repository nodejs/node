export class FastMap {
  private values: Object = {};

  delete(key: string): boolean {
    this.values[key] = null;
    return true;
  }

  set(key: string, value: any): FastMap {
    this.values[key] = value;
    return this;
  }

  get(key: string): any {
    return this.values[key];
  }

  forEach(cb: (value: any, key: any) => void, thisArg?: any): void {
    const values = this.values;
    for (let key in values) {
      if (values.hasOwnProperty(key) && values[key] !== null) {
        cb.call(thisArg, values[key], key);
      }
    }
  }

  clear(): void {
    this.values = {};
  }
}