// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that n-ary chains of binary ops give an equal result to individual
// binary op calls. Also test binop chains inside an if condition return
// the same branch.

// Generate a function of the form
//
// function(init,a0,...,aN) {
//   return init + a0 + ... + aN;
// }
//
// where + can be any binary operation.
function generate_chained_op(op, num_ops) {
    let str = "(function(init";
    for (let i = 0; i < num_ops; i++) {
        str += ",a"+i;
    }
    str += "){return (init";
    for (let i = 0; i < num_ops; i++) {
        str += op+"a"+i;
    }
    str += ");})";
    return eval(str);
}

// Generate a function of the form
//
// function(init,a0,...,aN) {
//   var tmp = init;
//   tmp = tmp + a0;
//   ...
//   tmp = tmp + aN;
//   return tmp;
// }
//
// where + can be any binary operation.
function generate_nonchained_op(op, num_ops) {
    let str = "(function(init";
    for (let i = 0; i < num_ops; i++) {
        str += ",a"+i;
    }
    str += "){ var tmp=init; ";
    for (let i = 0; i < num_ops; i++) {
        str += "tmp=(tmp"+op+"a"+i+");";
    }
    str += "return tmp;})";
    return eval(str);
}

// Generate a function of the form
//
// function(init,a0,...,aN) {
//   if(init + a0 + ... + aN) return 1;
//   else return 0;
// }
//
// where + can be any binary operation.
function generate_chained_op_test(op, num_ops) {
    let str = "(function(init";
    for (let i = 0; i < num_ops; i++) {
        str += ",a"+i;
    }
    str += "){ if(init";
    for (let i = 0; i < num_ops; i++) {
        str += op+"a"+i;
    }
    str += ")return 1;else return 0;})";
    return eval(str);
}

// Generate a function of the form
//
// function(init,a0,...,aN) {
//   var tmp = init;
//   tmp = tmp + a0;
//   ...
//   tmp = tmp + aN;
//   if(tmp) return 1
//  else return 0;
// }
//
// where + can be any binary operation.
function generate_nonchained_op_test(op, num_ops) {
    let str = "(function(init";
    for (let i = 0; i < num_ops; i++) {
        str += ",a"+i;
    }
    str += "){ var tmp=init; ";
    for (let i = 0; i < num_ops; i++) {
        str += "tmp=(tmp"+op+"a"+i+");";
    }
    str += "if(tmp)return 1;else return 0;})";
    return eval(str);
}

const BINOPS = [
    ",",
    "||",
    "&&",
    "|",
    "^",
    "&",
    "<<",
    ">>",
    ">>>",
    "+",
    "-",
    "*",
    "/",
    "%",
];

// Test each binop to see if the chained version is equivalent to the non-
// chained one.
for (let op of BINOPS) {
    let chained = generate_chained_op(op, 4);
    let nonchained = generate_nonchained_op(op, 4);
    let chained_test = generate_chained_op_test(op, 4);
    let nonchained_test = generate_nonchained_op_test(op, 4);

    // With numbers.
    assertEquals(
        nonchained(1,2,3,4,5),
        chained(1,2,3,4,5),
        "numeric " + op);

    // With numbers and strings.
    assertEquals(
        nonchained(1,"2",3,"4",5),
        chained(1,"2",3,"4",5),
        "numeric and string " + op);

    // Iterate over all possible combinations of 5 numbers that evaluate
    // to boolean true or false (for testing logical ops).
    for (var i = 0; i < 32; i++) {
        var booleanArray = [i & 1, i & 2, i & 4, i & 8, i & 16];
        assertEquals(
            nonchained.apply(this, booleanArray),
            chained.apply(this, booleanArray),
            booleanArray.join(" " + op + " "));

        assertEquals(
            nonchained_test.apply(this, booleanArray),
            chained_test.apply(this, booleanArray),
            "if (" + booleanArray.join(" " + op + " ") + ")");
    }
}
