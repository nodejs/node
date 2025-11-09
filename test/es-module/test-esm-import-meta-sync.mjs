import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { describe, it } from 'node:test';

describe('import.meta.sync', () => {
    it('should synchronously import a simple ESM module', () => {
        const simpleModule = import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-simple.mjs').href);
        assert.strictEqual(simpleModule.value, 42);
        assert.strictEqual(typeof simpleModule.greet, 'function');
        assert.strictEqual(simpleModule.greet('World'), 'Hello, World!');
    });

    it('should synchronously import a builtin module', () => {
        const fsModule = import.meta.sync('node:fs');
        assert.strictEqual(typeof fsModule.readFileSync, 'function');
        assert.strictEqual(typeof fsModule.readFile, 'function');
    });

    it('should throw ERR_MODULE_NOT_FOUND for non-existent modules', () => {
        assert.throws(() => {
            import.meta.sync(fixtures.fileURL('es-modules', 'does-not-exist.mjs').href);
        }, {
            code: 'ERR_MODULE_NOT_FOUND',
        });
    });

    it('should throw ERR_REQUIRE_ASYNC_MODULE for modules with TLA', () => {
        assert.throws(() => {
            import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-tla.mjs').href);
        }, {
            code: 'ERR_REQUIRE_ASYNC_MODULE',
        });
    });

    it('should return the same namespace for already loaded modules', () => {
        const simpleModule1 = import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-simple.mjs').href);
        const simpleModule2 = import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-simple.mjs').href);
        // Both should reference the same module namespace
        assert.strictEqual(simpleModule1.value, simpleModule2.value);
    });

    it('should synchronously import a CommonJS module', () => {
        const cjsModule = import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-cjs.cjs').href);
        assert.strictEqual(cjsModule.default.cjsValue, 'CJS works');
    });

    it('should handle relative path resolution', () => {
        const simpleModule = import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-simple.mjs').href);
        assert.ok(simpleModule);
        assert.strictEqual(simpleModule.value, 42);
    });

    it('should throw for modules that import modules with TLA', () => {
        // This tests that TLA detection works transitively through the module graph
        assert.throws(() => {
            import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-imports-tla.mjs').href);
        }, {
            code: 'ERR_REQUIRE_ASYNC_MODULE',
        });
    });

    it('should throw ERR_UNSUPPORTED_DIR_IMPORT for directory imports', () => {
        assert.throws(() => {
            import.meta.sync(fixtures.fileURL('es-modules').href);
        }, {
            code: 'ERR_UNSUPPORTED_DIR_IMPORT',
        });
    });

    it('should work with file path', () => {
        const filePath = fixtures.path('es-modules', 'test-esm-import-meta-sync-simple.mjs');
        const simpleModule = import.meta.sync(filePath);
        assert.strictEqual(simpleModule.value, 42);
        assert.strictEqual(typeof simpleModule.greet, 'function');
    });

    it('should work with relative paths', () => {
        const simpleModule = import.meta.sync('../fixtures/es-modules/test-esm-import-meta-sync-simple.mjs');
        assert.strictEqual(simpleModule.value, 42);
        assert.strictEqual(simpleModule.greet('Test'), 'Hello, Test!');
    });

    it('should not import extensionless modules', () => {
        const noext = import.meta.sync(fixtures.fileURL('es-modules', 'noext-esm').href);
        assert.ok(noext);
    });

    it('should synchronously import TypeScript modules', () => {
        const tsModule = import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-typescript.ts').href);
        assert.strictEqual(tsModule.tsValue, 100);
        assert.strictEqual(typeof tsModule.tsGreet, 'function');
        assert.strictEqual(tsModule.tsGreet('Node'), 'Hello from TypeScript, Node!');
    });

    it('should synchronously import a module that was already imported asynchronously', async () => {
        const url = fixtures.fileURL('es-modules', 'stateful.mjs').href;
        const asyncNs = await import(url);
        const syncNs = import.meta.sync(url);
        assert.strictEqual(syncNs, asyncNs);
    });

    it('should return the same namespace when imported sync first then async', async () => {
        const url = fixtures.fileURL('es-modules', 'test-esm-ok.mjs').href;
        const syncNs = import.meta.sync(url);
        const asyncNs = await import(url);
        assert.strictEqual(syncNs, asyncNs);
    });

    it('should return a Module Namespace Exotic Object', () => {
        const ns = import.meta.sync(fixtures.fileURL('es-modules', 'test-esm-import-meta-sync-simple.mjs').href);

        // Module namespace objects have Symbol.toStringTag === 'Module'
        assert.strictEqual(ns[Symbol.toStringTag], 'Module');
        assert.strictEqual(Object.isSealed(ns), true);
    });

    it('should have typeof "function"', () => {
        assert.strictEqual(typeof import.meta.sync, 'function');
    });

    it('should work with bare specifiers for builtins', () => {
        const fs1 = import.meta.sync('fs');
        const fs2 = import.meta.sync('node:fs');
        assert.strictEqual(fs1, fs2);
    });

    it('should coerce specifiers to string like import()', () => {
        assert.throws(() => {
            import.meta.sync(123);
        }, {
            code: 'ERR_MODULE_NOT_FOUND',
        });

        assert.throws(() => {
            import.meta.sync(null);
        }, {
            code: 'ERR_MODULE_NOT_FOUND',
        });

        assert.throws(() => {
            import.meta.sync(undefined);
        }, {
            code: 'ERR_MODULE_NOT_FOUND',
        });

        assert.throws(() => {
            import.meta.sync({});
        }, {
            code: 'ERR_MODULE_NOT_FOUND',
        });
    });

    describe('Module graph combinations (A->B->C)', () => {
        it('should import A->B->C when all are sync ESM', () => {
            const url = fixtures.fileURL('es-modules',
                'test-esm-import-meta-sync-a-esm-b-esm-c-sync.mjs');
            const a = import.meta.sync(url.href);
            assert.strictEqual(a.aValue, 'a-b-esm-sync-c');
        });

        it('should throw when A->B->C where C has TLA', () => {
            const url = fixtures.fileURL('es-modules',
                'test-esm-import-meta-sync-a-esm-b-esm-c-async.mjs');
            assert.throws(() => {
                import.meta.sync(url.href);
            }, {
                code: 'ERR_REQUIRE_ASYNC_MODULE',
            });
        });

        it('should import A(ESM)->B(CJS)', () => {
            const url = fixtures.fileURL('es-modules',
                'test-esm-import-meta-sync-a-esm-b-cjs.mjs');
            const a = import.meta.sync(url.href);
            assert.strictEqual(a.aValue, 'a-b-cjs-sync');
        });

        it('should throw when B(ESM)->C(async)', () => {
            const url = fixtures.fileURL('es-modules',
                'test-esm-import-meta-sync-b-esm-async.mjs');
            assert.throws(() => {
                import.meta.sync(url.href);
            }, {
                code: 'ERR_REQUIRE_ASYNC_MODULE',
            });
        });

        it('should import B(ESM)->C(sync)', () => {
            const url = fixtures.fileURL('es-modules',
                'test-esm-import-meta-sync-b-esm-sync.mjs');
            const b = import.meta.sync(url.href);
            assert.strictEqual(b.bValue, 'b-esm-sync-c');
        });

        it('should import standalone CJS module', () => {
            const url = fixtures.fileURL('es-modules',
                'test-esm-import-meta-sync-b-cjs.cjs');
            const b = import.meta.sync(url.href);
            assert.strictEqual(b.default.bValue, 'b-cjs-sync');
        });
    });
});
