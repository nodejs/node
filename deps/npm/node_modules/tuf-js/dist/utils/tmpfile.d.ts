type TempFileHandler<T> = (file: string) => Promise<T>;
export declare const withTempFile: <T>(handler: TempFileHandler<T>) => Promise<T>;
export {};
