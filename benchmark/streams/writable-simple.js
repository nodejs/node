'use strict';

const common = require('../common');
const Writable = require('stream').Writable;

const bench = common.createBenchmark(main, {
  n: [50000],
  inputType: ['buffer', 'string'],
  inputEncoding: ['utf8', 'ucs2'],
  decodeAs: ['', 'utf8', 'ucs2']
});

const inputStr = `袎逜 釂鱞鸄 碄碆碃 蒰裧頖 鋑, 瞂 覮轀 蔝蓶蓨 踥踕踛 鬐鶤 鄜 忁曨曣
翀胲胵, 鬵鵛嚪 釢髟偛 碞碠粻 漀 涾烰 跬 窱縓 墥墡嬇 禒箈箑, 餤駰 瀁瀎瀊 躆轖轕 蒛, 銙 簎艜薤
樆樦潏 魡鳱 櫱瀯灂 鷜鷙鷵 禒箈箑 綧 駓駗, 鋡 嗛嗕塨 嶭嶴憝 爂犤繵 罫蓱 摮 灉礭蘠 蠬襱覾 脬舑莕
躐鑏, 襆贂 漀 刲匊呥 肒芅邥 泏狔狑, 瀗犡礝 浘涀缹 輲輹 綧`;

function main(conf) {
  const n = +conf.n;
  const s = new Writable({
    decodeBuffers: !!conf.decodeAs,
    defaultEncoding: conf.decodeAs || undefined,
    write(chunk, encoding, cb) { cb(); }
  });

  const inputEnc = conf.inputType === 'buffer' ? undefined : conf.inputEncoding;
  const input = conf.inputType === 'buffer' ?
      Buffer.from(inputStr, conf.inputEncoding) : inputStr;

  bench.start();
  for (var k = 0; k < n; ++k) {
    s.write(input, inputEnc);
  }
  bench.end(n);
}
