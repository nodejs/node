n=require('net')

n.connect(7001, '127.0.0.1').on('connect', function() {
  console.log(this.address())
  this.destroy();
});
