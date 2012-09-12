
require(__dirname).test({
  xml :
  "<root>"+
    "<child>" +
      "<haha />" +
    "</child>" +
    "<monkey>" +
      "=(|)" +
    "</monkey>" +
  "</root>",
  expect : [
    ["opentag", {
     "name": "ROOT",
     "attributes": {}
    }],
    ["opentag", {
     "name": "CHILD",
     "attributes": {}
    }],
    ["opentag", {
     "name": "HAHA",
     "attributes": {}
    }],
    ["closetag", "HAHA"],
    ["closetag", "CHILD"],
    ["opentag", {
     "name": "MONKEY",
     "attributes": {}
    }],
    ["text", "=(|)"],
    ["closetag", "MONKEY"],
    ["closetag", "ROOT"],
    ["end"],
    ["ready"]
  ],
  strict : false,
  opt : {}
});

