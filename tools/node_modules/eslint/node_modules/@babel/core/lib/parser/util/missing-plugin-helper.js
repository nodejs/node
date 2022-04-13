"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = generateMissingPluginMessage;
const pluginNameMap = {
  asyncDoExpressions: {
    syntax: {
      name: "@babel/plugin-syntax-async-do-expressions",
      url: "https://git.io/JYer8"
    }
  },
  classProperties: {
    syntax: {
      name: "@babel/plugin-syntax-class-properties",
      url: "https://git.io/vb4yQ"
    },
    transform: {
      name: "@babel/plugin-proposal-class-properties",
      url: "https://git.io/vb4SL"
    }
  },
  classPrivateProperties: {
    syntax: {
      name: "@babel/plugin-syntax-class-properties",
      url: "https://git.io/vb4yQ"
    },
    transform: {
      name: "@babel/plugin-proposal-class-properties",
      url: "https://git.io/vb4SL"
    }
  },
  classPrivateMethods: {
    syntax: {
      name: "@babel/plugin-syntax-class-properties",
      url: "https://git.io/vb4yQ"
    },
    transform: {
      name: "@babel/plugin-proposal-private-methods",
      url: "https://git.io/JvpRG"
    }
  },
  classStaticBlock: {
    syntax: {
      name: "@babel/plugin-syntax-class-static-block",
      url: "https://git.io/JTLB6"
    },
    transform: {
      name: "@babel/plugin-proposal-class-static-block",
      url: "https://git.io/JTLBP"
    }
  },
  decimal: {
    syntax: {
      name: "@babel/plugin-syntax-decimal",
      url: "https://git.io/JfKOH"
    }
  },
  decorators: {
    syntax: {
      name: "@babel/plugin-syntax-decorators",
      url: "https://git.io/vb4y9"
    },
    transform: {
      name: "@babel/plugin-proposal-decorators",
      url: "https://git.io/vb4ST"
    }
  },
  doExpressions: {
    syntax: {
      name: "@babel/plugin-syntax-do-expressions",
      url: "https://git.io/vb4yh"
    },
    transform: {
      name: "@babel/plugin-proposal-do-expressions",
      url: "https://git.io/vb4S3"
    }
  },
  dynamicImport: {
    syntax: {
      name: "@babel/plugin-syntax-dynamic-import",
      url: "https://git.io/vb4Sv"
    }
  },
  exportDefaultFrom: {
    syntax: {
      name: "@babel/plugin-syntax-export-default-from",
      url: "https://git.io/vb4SO"
    },
    transform: {
      name: "@babel/plugin-proposal-export-default-from",
      url: "https://git.io/vb4yH"
    }
  },
  exportNamespaceFrom: {
    syntax: {
      name: "@babel/plugin-syntax-export-namespace-from",
      url: "https://git.io/vb4Sf"
    },
    transform: {
      name: "@babel/plugin-proposal-export-namespace-from",
      url: "https://git.io/vb4SG"
    }
  },
  flow: {
    syntax: {
      name: "@babel/plugin-syntax-flow",
      url: "https://git.io/vb4yb"
    },
    transform: {
      name: "@babel/preset-flow",
      url: "https://git.io/JfeDn"
    }
  },
  functionBind: {
    syntax: {
      name: "@babel/plugin-syntax-function-bind",
      url: "https://git.io/vb4y7"
    },
    transform: {
      name: "@babel/plugin-proposal-function-bind",
      url: "https://git.io/vb4St"
    }
  },
  functionSent: {
    syntax: {
      name: "@babel/plugin-syntax-function-sent",
      url: "https://git.io/vb4yN"
    },
    transform: {
      name: "@babel/plugin-proposal-function-sent",
      url: "https://git.io/vb4SZ"
    }
  },
  importMeta: {
    syntax: {
      name: "@babel/plugin-syntax-import-meta",
      url: "https://git.io/vbKK6"
    }
  },
  jsx: {
    syntax: {
      name: "@babel/plugin-syntax-jsx",
      url: "https://git.io/vb4yA"
    },
    transform: {
      name: "@babel/preset-react",
      url: "https://git.io/JfeDR"
    }
  },
  importAssertions: {
    syntax: {
      name: "@babel/plugin-syntax-import-assertions",
      url: "https://git.io/JUbkv"
    }
  },
  moduleStringNames: {
    syntax: {
      name: "@babel/plugin-syntax-module-string-names",
      url: "https://git.io/JTL8G"
    }
  },
  numericSeparator: {
    syntax: {
      name: "@babel/plugin-syntax-numeric-separator",
      url: "https://git.io/vb4Sq"
    },
    transform: {
      name: "@babel/plugin-proposal-numeric-separator",
      url: "https://git.io/vb4yS"
    }
  },
  optionalChaining: {
    syntax: {
      name: "@babel/plugin-syntax-optional-chaining",
      url: "https://git.io/vb4Sc"
    },
    transform: {
      name: "@babel/plugin-proposal-optional-chaining",
      url: "https://git.io/vb4Sk"
    }
  },
  pipelineOperator: {
    syntax: {
      name: "@babel/plugin-syntax-pipeline-operator",
      url: "https://git.io/vb4yj"
    },
    transform: {
      name: "@babel/plugin-proposal-pipeline-operator",
      url: "https://git.io/vb4SU"
    }
  },
  privateIn: {
    syntax: {
      name: "@babel/plugin-syntax-private-property-in-object",
      url: "https://git.io/JfK3q"
    },
    transform: {
      name: "@babel/plugin-proposal-private-property-in-object",
      url: "https://git.io/JfK3O"
    }
  },
  recordAndTuple: {
    syntax: {
      name: "@babel/plugin-syntax-record-and-tuple",
      url: "https://git.io/JvKp3"
    }
  },
  regexpUnicodeSets: {
    syntax: {
      name: "@babel/plugin-syntax-unicode-sets-regex",
      url: "https://git.io/J9GTd"
    },
    transform: {
      name: "@babel/plugin-proposal-unicode-sets-regex",
      url: "https://git.io/J9GTQ"
    }
  },
  throwExpressions: {
    syntax: {
      name: "@babel/plugin-syntax-throw-expressions",
      url: "https://git.io/vb4SJ"
    },
    transform: {
      name: "@babel/plugin-proposal-throw-expressions",
      url: "https://git.io/vb4yF"
    }
  },
  typescript: {
    syntax: {
      name: "@babel/plugin-syntax-typescript",
      url: "https://git.io/vb4SC"
    },
    transform: {
      name: "@babel/preset-typescript",
      url: "https://git.io/JfeDz"
    }
  },
  asyncGenerators: {
    syntax: {
      name: "@babel/plugin-syntax-async-generators",
      url: "https://git.io/vb4SY"
    },
    transform: {
      name: "@babel/plugin-proposal-async-generator-functions",
      url: "https://git.io/vb4yp"
    }
  },
  logicalAssignment: {
    syntax: {
      name: "@babel/plugin-syntax-logical-assignment-operators",
      url: "https://git.io/vAlBp"
    },
    transform: {
      name: "@babel/plugin-proposal-logical-assignment-operators",
      url: "https://git.io/vAlRe"
    }
  },
  nullishCoalescingOperator: {
    syntax: {
      name: "@babel/plugin-syntax-nullish-coalescing-operator",
      url: "https://git.io/vb4yx"
    },
    transform: {
      name: "@babel/plugin-proposal-nullish-coalescing-operator",
      url: "https://git.io/vb4Se"
    }
  },
  objectRestSpread: {
    syntax: {
      name: "@babel/plugin-syntax-object-rest-spread",
      url: "https://git.io/vb4y5"
    },
    transform: {
      name: "@babel/plugin-proposal-object-rest-spread",
      url: "https://git.io/vb4Ss"
    }
  },
  optionalCatchBinding: {
    syntax: {
      name: "@babel/plugin-syntax-optional-catch-binding",
      url: "https://git.io/vb4Sn"
    },
    transform: {
      name: "@babel/plugin-proposal-optional-catch-binding",
      url: "https://git.io/vb4SI"
    }
  }
};
pluginNameMap.privateIn.syntax = pluginNameMap.privateIn.transform;

const getNameURLCombination = ({
  name,
  url
}) => `${name} (${url})`;

function generateMissingPluginMessage(missingPluginName, loc, codeFrame) {
  let helpMessage = `Support for the experimental syntax '${missingPluginName}' isn't currently enabled ` + `(${loc.line}:${loc.column + 1}):\n\n` + codeFrame;
  const pluginInfo = pluginNameMap[missingPluginName];

  if (pluginInfo) {
    const {
      syntax: syntaxPlugin,
      transform: transformPlugin
    } = pluginInfo;

    if (syntaxPlugin) {
      const syntaxPluginInfo = getNameURLCombination(syntaxPlugin);

      if (transformPlugin) {
        const transformPluginInfo = getNameURLCombination(transformPlugin);
        const sectionType = transformPlugin.name.startsWith("@babel/plugin") ? "plugins" : "presets";
        helpMessage += `\n\nAdd ${transformPluginInfo} to the '${sectionType}' section of your Babel config to enable transformation.
If you want to leave it as-is, add ${syntaxPluginInfo} to the 'plugins' section to enable parsing.`;
      } else {
        helpMessage += `\n\nAdd ${syntaxPluginInfo} to the 'plugins' section of your Babel config ` + `to enable parsing.`;
      }
    }
  }

  return helpMessage;
}