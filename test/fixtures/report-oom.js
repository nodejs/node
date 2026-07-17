'use strict';

const list = [];
while (true) {
  const record = new MyRecord();
  list.push(record);
}

function MyRecord() {
  this.name = 'foo';
  this.id = 128;
  this.account = 98454324;
}
