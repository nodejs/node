include("mjsunit.js");

// üäö

puts("Σὲ γνωρίζω ἀπὸ τὴν κόψη");

function onLoad () {
  assertTrue( /Hellö Wörld/.test("Hellö Wörld") ); 
}

