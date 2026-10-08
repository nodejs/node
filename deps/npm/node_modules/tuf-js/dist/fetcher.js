"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.DefaultFetcher = exports.BaseFetcher = void 0;
const debug_1 = __importDefault(require("debug"));
const fs_1 = __importDefault(require("fs"));
const util_1 = __importDefault(require("util"));
const error_1 = require("./error");
const tmpfile_1 = require("./utils/tmpfile");
const promise_retry_1 = require("@gar/promise-retry");
const log = (0, debug_1.default)('tuf:fetch');
const USER_AGENT_HEADER = 'User-Agent';
class BaseFetcher {
    // Download file from given URL. The file is downloaded to a temporary
    // location and then passed to the given handler. The handler is responsible
    // for moving the file to its final location. The temporary file is deleted
    // after the handler returns.
    async downloadFile(url, maxLength, handler) {
        return (0, tmpfile_1.withTempFile)(async (tmpFile) => {
            const reader = await this.fetch(url);
            let numberOfBytesReceived = 0;
            const fileStream = fs_1.default.createWriteStream(tmpFile);
            // Read the stream a chunk at a time so that we can check
            // the length of the file as we go
            const streamReader = reader.getReader();
            try {
                while (true) {
                    const { done, value: chunk } = await streamReader.read();
                    if (done) {
                        break;
                    }
                    numberOfBytesReceived += chunk.length;
                    if (numberOfBytesReceived > maxLength) {
                        throw new error_1.DownloadLengthMismatchError('Max length reached');
                    }
                    await writeBufferToStream(fileStream, Buffer.from(chunk));
                }
            }
            finally {
                streamReader.releaseLock();
                // Make sure we always close the stream
                // eslint-disable-next-line @typescript-eslint/unbound-method
                await util_1.default.promisify(fileStream.close).bind(fileStream)();
            }
            return handler(tmpFile);
        });
    }
    // Download bytes from given URL.
    async downloadBytes(url, maxLength) {
        return this.downloadFile(url, maxLength, async (file) => {
            const stream = fs_1.default.createReadStream(file);
            const chunks = [];
            for await (const chunk of stream) {
                chunks.push(chunk);
            }
            return Buffer.concat(chunks);
        });
    }
}
exports.BaseFetcher = BaseFetcher;
class DefaultFetcher extends BaseFetcher {
    userAgent;
    timeout;
    retry;
    constructor(options = {}) {
        super();
        this.userAgent = options.userAgent;
        this.timeout = options.timeout;
        // Map retry to OperationOptions
        if (options.retry === true) {
            this.retry = { forever: true };
        }
        else if (options.retry === false || options.retry === undefined) {
            this.retry = undefined;
        }
        else if (typeof options.retry === 'number') {
            if (options.retry < 0) {
                throw new Error('Retry count must be non-negative number');
            }
            this.retry = { retries: options.retry };
        }
        else {
            this.retry = options.retry;
        }
    }
    async fetch(url) {
        const shouldRetry = this.retry !== undefined;
        return (0, promise_retry_1.promiseRetry)(async (retry, number) => {
            log('GET %s (attempt %d)', url, number);
            let response;
            try {
                response = await fetch(url, {
                    headers: {
                        [USER_AGENT_HEADER]: this.userAgent || '',
                    },
                    signal: this.timeout
                        ? AbortSignal.timeout(this.timeout)
                        : undefined,
                });
            }
            catch (error) {
                const err = error instanceof Error ? error : new Error(String(error));
                if (shouldRetry) {
                    return retry(err);
                }
                throw err;
            }
            if (!response.ok || !response.body) {
                const err = new error_1.DownloadHTTPError('Failed to download', response.status);
                if (shouldRetry && response.status >= 500 && response.status < 600) {
                    return retry(err);
                }
                throw err;
            }
            return response.body;
        }, this.retry);
    }
}
exports.DefaultFetcher = DefaultFetcher;
const writeBufferToStream = async (stream, buffer) => {
    return new Promise((resolve, reject) => {
        stream.write(buffer, (err) => {
            if (err) {
                reject(err);
            }
            resolve(true);
        });
    });
};
