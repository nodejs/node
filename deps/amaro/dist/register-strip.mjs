import module from "node:module";

if (typeof module.registerHooks === "function") {
	const { default: loader } = await import("./strip-loader.js");
	module.registerHooks({ load: loader.loadSync });
} else {
	// Node.js versions without `module.registerHooks()`.
	module.register("./strip-loader.js", import.meta.url);
}
