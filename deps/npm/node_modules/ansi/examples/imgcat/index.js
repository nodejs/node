#!/usr/bin/env node

process.title = 'imgcat'

var ansi = require('../../')
  , cursor = ansi(process.stdout, { enabled: true })
  , tty = require('tty')
  , Canvas = require('canvas')
  , imageFile = process.argv[2] || __dirname + '/yoshi.png'
  , screenWidth = process.stdout.isTTY ? process.stdout.getWindowSize()[0] : Infinity
  , maxWidth = parseInt(process.argv[3], 10) || screenWidth
  , image = require('fs').readFileSync(imageFile)
  , pixel = '  '
  , alphaThreshold = 0

var img = new Canvas.Image();
img.src = image;

function draw () {
  var width = maxWidth / pixel.length
    , scaleW = img.width > width ? width / img.width : 1
    , w = Math.floor(img.width * scaleW)
    , h = Math.floor(img.height * scaleW);

  var canvas = new Canvas(w, h)
    , ctx = canvas.getContext('2d');

  ctx.drawImage(img, 0, 0, w, h);

  var data = ctx.getImageData(0, 0, w, h).data;

  for (var i=0, l=data.length; i<l; i+=4) {
    var r = data[i]
      , g = data[i+1]
      , b = data[i+2]
      , alpha = data[i+3];
    if (alpha > alphaThreshold) {
      cursor.bg.rgb(r, g, b);
    } else {
      cursor.bg.reset();
    }
    process.stdout.write(pixel);
    if ((i/4|0) % w === (w-1)) {
      cursor.bg.reset();
      process.stdout.write('\n');
    }
  }
}

draw();
