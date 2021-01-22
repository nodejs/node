'use strict';

// Exercise coverage of a class. Note, this logic is silly and exists solely
// to generate branch coverage code paths:
class CoveredClass {
  constructor(x, y, opts) {
    this.x = x;
    this.y = y;
    // Exercise coverage of nullish coalescing:
    this.opts = opts ?? (Math.random() > 0.5 ? {} : undefined);
  }
  add() {
    return this.x + this.y;
  }
  addSpecial() {
    // Exercise coverage of optional chains:
    if (this.opts?.special && this.opts?.special?.x && this.opts?.special?.y) {
      return this.opts.special.x + this.opts.special.y;
    }
    return add();
  }
  mult() {
    return this.x * this.y;
  }
  multSpecial() {
    if (this.opts?.special && this.opts?.special?.x && this.opts?.special?.y) {
      return this.opts.special.x * this.opts.special.y;
    }
    return mult();
  }
}

// Excercise coverage of functions:
function add(x, y) {
  const mt = new CoveredClass(x, y);
  return mt.add();
}

function addSpecial(x, y) {
  let mt;
  if (Math.random() > 0.5) {
    mt = new CoveredClass(x, y);
  } else {
    mt = new CoveredClass(x, y, {
      special: {
        x: Math.random() * x,
        y: Math.random() * y
      }
    });
  }
  return mt.addSpecial();
}

function mult(x, y) {
  const mt = new CoveredClass(x, y);
  return mt.mult();
}

function multSpecial(x, y) {
  let mt;
  if (Math.random() > 0.5) {
    mt = new CoveredClass(x, y);
  } else {
    mt = new CoveredClass(x, y, {
      special: {
        x: Math.random() * x,
        y: Math.random() * y
      }
    });
  }
  return mt.multSpecial();
}

for (let i = 0; i < parseInt(process.env.N); i++) {
  const operations = ['add', 'addSpecial', 'mult', 'multSpecial'];
  for (const operation of operations) {
    // Exercise coverage of switch statements:
    switch (operation) {
      case 'add':
        if (add(Math.random() * 10, Math.random() * 10) > 10) {
          // Exercise coverage of ternary operations:
          let r = addSpecial(Math.random() * 10, Math.random() * 10) > 10 ?
            mult(Math.random() * 10, Math.random() * 10) :
            add(Math.random() * 10, Math.random() * 10);
          // Exercise && and ||
          if (r && Math.random() > 0.5 || Math.random() < 0.5) r++;
        }
        break;
      case 'addSpecial':
        if (addSpecial(Math.random() * 10, Math.random() * 10) > 10 &&
            add(Math.random() * 10, Math.random() * 10) > 10) {
          let r = mult(Math.random() * 10, Math.random() * 10) > 10 ?
            add(Math.random() * 10, Math.random() * 10) > 10 :
            mult(Math.random() * 10, Math.random() * 10);
          if (r && Math.random() > 0.5 || Math.random() < 0.5) r++;
        }
        break;
      case 'mult':
        if (mult(Math.random() * 10, Math.random() * 10) > 10) {
          let r = multSpecial(Math.random() * 10, Math.random() * 10) > 10 ?
            add(Math.random() * 10, Math.random() * 10) :
            mult(Math.random() * 10, Math.random() * 10);
          if (r && Math.random() > 0.5 || Math.random() < 0.5) r++;
        }
        break;
      case 'multSpecial':
        while (multSpecial(Math.random() * 10, Math.random() * 10) < 10) {
          mult(Math.random() * 10, Math.random() * 10);
        }
        break;
      default:
        break;
    }
  }
}
