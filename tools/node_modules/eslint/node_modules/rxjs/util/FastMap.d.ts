export declare class FastMap {
    private values;
    delete(key: string): boolean;
    set(key: string, value: any): FastMap;
    get(key: string): any;
    forEach(cb: (value: any, key: any) => void, thisArg?: any): void;
    clear(): void;
}
