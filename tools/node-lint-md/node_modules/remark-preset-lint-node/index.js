// @see https://github.com/nodejs/node/blob/HEAD/doc/guides/doc-style-guide.md

import remarkPresetLintRecommended from "remark-preset-lint-recommended";
import remarkLintBlockquoteIndentation from "remark-lint-blockquote-indentation";
import remarkLintCheckboxCharacterStyle from "remark-lint-checkbox-character-style";
import remarkLintCheckboxContentIndent from "remark-lint-checkbox-content-indent";
import remarkLintCodeBlockStyle from "remark-lint-code-block-style";
import remarkLintDefinitionSpacing from "remark-lint-definition-spacing";
import remarkLintFencedCodeFlag from "remark-lint-fenced-code-flag";
import remarkLintFencedCodeMarker from "remark-lint-fenced-code-marker";
import remarkLintFileExtension from "remark-lint-file-extension";
import remarkLintFinalDefinition from "remark-lint-final-definition";
import remarkLintFirstHeadingLevel from "remark-lint-first-heading-level";
import remarkLintHeadingStyle from "remark-lint-heading-style";
import remarkLintListItemIndent from "remark-lint-list-item-indent";
import remarkLintMaximumLineLength from "remark-lint-maximum-line-length";
import remarkLintNoConsecutiveBlankLines from "remark-lint-no-consecutive-blank-lines";
import remarkLintNoFileNameArticles from "remark-lint-no-file-name-articles";
import remarkLintNoFileNameConsecutiveDashes from "remark-lint-no-file-name-consecutive-dashes";
import remarkLintNofileNameOuterDashes from "remark-lint-no-file-name-outer-dashes";
import remarkLintNoHeadingIndent from "remark-lint-no-heading-indent";
import remarkLintNoMultipleToplevelHeadings from "remark-lint-no-multiple-toplevel-headings";
import remarkLintNoShellDollars from "remark-lint-no-shell-dollars";
import remarkLintNoTableIndentation from "remark-lint-no-table-indentation";
import remarkLintNoTabs from "remark-lint-no-tabs";
import remarkLintNoTrailingSpaces from "remark-lint-no-trailing-spaces";
import remarkLintNodejsLinks from "./remark-lint-nodejs-links.js";
import remarkLintNodejsYamlComments from "./remark-lint-nodejs-yaml-comments.js";
import remarkLintProhibitedStrings from "remark-lint-prohibited-strings";
import remarkLintRuleStyle from "remark-lint-rule-style";
import remarkLintStrongMarker from "remark-lint-strong-marker";
import remarkLintTableCellPadding from "remark-lint-table-cell-padding";
import remarkLintTablePipes from "remark-lint-table-pipes";
import remarkLintUnorderedListMarkerStyle from "remark-lint-unordered-list-marker-style";

// Add in rules alphabetically
const plugins = [
  // Leave preset at the top so it can be overridden
  remarkPresetLintRecommended,
  [remarkLintBlockquoteIndentation, 2],
  [remarkLintCheckboxCharacterStyle, { checked: "x", unchecked: " " }],
  remarkLintCheckboxContentIndent,
  [remarkLintCodeBlockStyle, "fenced"],
  remarkLintDefinitionSpacing,
  [
    remarkLintFencedCodeFlag,
    {
      flags: [
        "bash",
        "c",
        "cjs",
        "coffee",
        "console",
        "cpp",
        "diff",
        "http",
        "js",
        "json",
        "markdown",
        "mjs",
        "powershell",
        "r",
        "text",
      ],
    },
  ],
  [remarkLintFencedCodeMarker, "`"],
  [remarkLintFileExtension, "md"],
  remarkLintFinalDefinition,
  [remarkLintFirstHeadingLevel, 1],
  [remarkLintHeadingStyle, "atx"],
  [remarkLintListItemIndent, "space"],
  remarkLintMaximumLineLength,
  remarkLintNoConsecutiveBlankLines,
  remarkLintNoFileNameArticles,
  remarkLintNoFileNameConsecutiveDashes,
  remarkLintNofileNameOuterDashes,
  remarkLintNoHeadingIndent,
  remarkLintNoMultipleToplevelHeadings,
  remarkLintNoShellDollars,
  remarkLintNoTableIndentation,
  remarkLintNoTabs,
  remarkLintNoTrailingSpaces,
  remarkLintNodejsLinks,
  remarkLintNodejsYamlComments,
  [
    remarkLintProhibitedStrings,
    [
      { yes: "End-of-Life" },
      { yes: "GitHub" },
      { no: "hostname", yes: "host name" },
      { yes: "JavaScript" },
      { no: "[Ll]ong[ -][Tt]erm [Ss]upport", yes: "Long Term Support" },
      { no: "Node", yes: "Node.js", ignoreNextTo: "-API" },
      { yes: "Node.js" },
      { no: "Node[Jj][Ss]", yes: "Node.js" },
      { no: "Node\\.js's?", yes: "the Node.js" },
      { no: "[Nn]ote that", yes: "<nothing>" },
      { yes: "RFC" },
      { no: "[Rr][Ff][Cc]\\d+", yes: "RFC <number>" },
      { yes: "Unix" },
      { yes: "V8" },
    ],
  ],
  remarkLintRuleStyle,
  [remarkLintStrongMarker, "*"],
  [remarkLintTableCellPadding, "padded"],
  remarkLintTablePipes,
  [remarkLintUnorderedListMarkerStyle, "*"],
];

const remarkPresetLintNode = { plugins };

export default remarkPresetLintNode;
