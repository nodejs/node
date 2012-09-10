
require(__dirname).test({
  expect : [
    ["opentag", {"name": "R","attributes": {}}],
    ["opencdata", undefined],
    ["cdata", " this is character data  "],
    ["closecdata", undefined],
    ["closetag", "R"]
  ]
}).write("<r><![CDATA[ this is ").write("character data  ").write("]]></r>").close();

