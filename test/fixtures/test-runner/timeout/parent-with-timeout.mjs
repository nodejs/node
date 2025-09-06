import { test } from "node:test";
import { setTimeout } from "node:timers/promises";

test("base test", { timeout: 60000 }, async () => {
    await test("should not fail", async () => {
        await setTimeout(500);
    })
});
