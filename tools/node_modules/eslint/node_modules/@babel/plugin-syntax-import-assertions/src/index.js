import { declare } from "@babel/helper-plugin-utils";

export default declare(api => {
  api.assertVersion(7);

  return {
    name: "syntax-import-assertions",

    manipulateOptions(opts, parserOpts) {
      parserOpts.plugins.push(["importAssertions"]);
    },
  };
});
