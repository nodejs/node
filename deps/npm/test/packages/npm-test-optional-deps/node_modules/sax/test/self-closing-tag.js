
require(__dirname).test({
  xml :
  "<root>   "+
    "<haha /> "+
    "<haha/>  "+
    "<monkey> "+
      "=(|)     "+
    "</monkey>"+
  "</root>  ",
  expect : [
    ["opentag", {name:"ROOT", attributes:{}}],
    ["opentag", {name:"HAHA", attributes:{}}],
    ["closetag", "HAHA"],
    ["opentag", {name:"HAHA", attributes:{}}],
    ["closetag", "HAHA"],
    // ["opentag", {name:"HAHA", attributes:{}}],
    // ["closetag", "HAHA"],
    ["opentag", {name:"MONKEY", attributes:{}}],
    ["text", "=(|)"],
    ["closetag", "MONKEY"],
    ["closetag", "ROOT"]
  ],
  opt : { trim : true }
});