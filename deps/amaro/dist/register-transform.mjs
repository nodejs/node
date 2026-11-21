import module from "node:module";
import { emitWarning, env, execArgv } from "node:process";

const hasSourceMaps =
	execArgv.includes("--enable-source-maps") ||
	env.NODE_OPTIONS?.includes("--enable-source-maps");

if (!hasSourceMaps) {
	emitWarning("Source maps are disabled, stack traces will not be accurate");
}

if (typeof module.registerHooks === "function") {
	const { default: loader } = await import("./transform-loader.js");
	module.registerHooks({ load: loader.loadSync });
} else {
	// Node.js versions without `module.registerHooks()`.
	module.register("./transform-loader.js", import.meta.url);
}
