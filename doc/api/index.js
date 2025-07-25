const TelegramBot = require('node-telegram-bot-api');
const TOKEN = '7608172292:AAEF-C_anOFrZZz9ckUMKsslpytVL5KvkeY'; //
const CHANNEL = '-1007631781865'; // 

const bot = new TelegramBot(TOKEN, { polling: false });

function gerarSinal() {
  const bombas = 3;
  const casas = [];
  while (casas.length < 3) {
    const num = Math.floor(Math.random() * 25) + 1;
    if (!casas.includes(num)) casas.push(num);
  }
  casas.sort((a, b) => a - b);

  const msg = `
🎯 *NOVO SINAL MINES*
💣 Bombas: *${bombas}*
📦 Casas: *${casas.join(', ')}*
✅ Estratégia: *2x1*
⏰ ${new Date().toLocaleTimeString()}
`.trim();

  return msg;
}

function enviarSinal() {
  const mensagem = gerarSinal();
  bot.sendMessage(CHANNEL, mensagem, { parse_mode: 'Markdown' })
    .then(() => console.log('✅ Sinal enviado'))
    .catch((err) => console.error('Erro ao enviar:', err));
}

// Enviar sinal a cada 5 minutos
setInterval(enviarSinal, 5 * 60 * 1000);

console.log('🤖 Bot de sinais iniciado!');
node index.js
