/***
 * Node External Editor
 *
 * Kevin Gravier <kevin@mrkmg.com>
 * MIT 2018
 */
import { CreateFileError } from "./errors/CreateFileError";
import { LaunchEditorError } from "./errors/LaunchEditorError";
import { ReadFileError } from "./errors/ReadFileError";
import { RemoveFileError } from "./errors/RemoveFileError";
export interface IEditorParams {
    args: string[];
    bin: string;
}
export declare type StringCallback = (err: Error, result: string) => void;
export declare type VoidCallback = () => void;
export { CreateFileError, LaunchEditorError, ReadFileError, RemoveFileError };
export declare function edit(text?: string): string;
export declare function editAsync(text: string, callback: StringCallback): void;
export declare class ExternalEditor {
    private static splitStringBySpace;
    text: string;
    tempFile: string;
    editor: IEditorParams;
    lastExitStatus: number;
    readonly temp_file: string;
    readonly last_exit_status: number;
    constructor(text?: string);
    run(): string;
    runAsync(callback: StringCallback): void;
    cleanup(): void;
    private determineEditor;
    private createTemporaryFile;
    private readTemporaryFile;
    private removeTemporaryFile;
    private launchEditor;
    private launchEditorAsync;
}
