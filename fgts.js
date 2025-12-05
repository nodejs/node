// ===============================
// CONFIGURAÇÕES INICIAIS
// ===============================
const AUTH_URL = "https://auth.v8sistema.com/oauth/token";
const API_URL = "https://bff.v8sistema.com";

// 🚨 Preencha com seus dados
const CLIENT_ID = "DHWogdaYmEI8n5bwwxPDzulMlSK7dwIn";
const USER_EMAIL = "SEU_EMAIL_AQUI";
const USER_PASSWORD = "SUA_SENHA_AQUI";

// ===============================
// 1. AUTENTICAÇÃO
// ===============================
async function autenticar() {
  const res = await fetch(AUTH_URL, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      client_id: CLIENT_ID,
      audience: "https://bff.v8sistema.com",
      scope: "offline_access",
      username: USER_EMAIL,
      password: USER_PASSWORD,
      grant_type: "password"
    })
  });

  const data = await res.json();
  if (!data.access_token) {
    console.log("❌ Erro ao autenticar:", data);
    return null;
  }

  console.log("🔑 Token obtido com sucesso!");
  return data.access_token;
}

// ===============================
// 2. CONSULTA DE SALDO FGTS
// ===============================
async function consultarSaldoFGTS(cpf, provider = "bms") {
  const token = await autenticar();
  if (!token) return;

  const res = await fetch(`${API_URL}/fgts/balance`, {
    method: "POST",
    headers: {
      "Authorization": `Bearer ${token}`,
      "Content-Type": "application/json"
    },
    body: JSON.stringify({
      documentNumber: cpf,
      provider
    })
  });

  const data = await res.json();
  console.log("🔍 Consulta iniciada:", data);

  return data;
}

// ===============================
// 3. BUSCAR RESULTADO DA CONSULTA
// ===============================
async function buscarResultadoSaldo(cpf) {
  const token = await autenticar();
  if (!token) return;

  const res = await fetch(`${API_URL}/fgts/balance?search=${cpf}`, {
    method: "GET",
    headers: { "Authorization": `Bearer ${token}` }
  });

  const data = await res.json();
  console.log("📊 Resultado do saldo FGTS:", data);

  return data;
}

// ===============================
// 4. CRIAR PROPOSTA FGTS
// ===============================
async function criarPropostaFGTS(payload) {
  const token = await autenticar();
  if (!token) return;

  const res = await fetch(`${API_URL}/fgts/proposal`, {
    method: "POST",
    headers: {
      "Authorization": `Bearer ${token}`,
      "Content-Type": "application/json"
    },
    body: JSON.stringify(payload)
  });

  const data = await res.json();
  console.log("📝 Proposta criada:", data);

  return data;
}

// ===============================
// 5. LISTAR PROPOSTAS POR CPF
// ===============================
async function listarPropostas(cpf, status = "") {
  const token = await autenticar();
  if (!token) return;

  const url = `${API_URL}/fgts/proposal?search=${cpf}&status=${status}&page=1&limit=20`;

  const res = await fetch(url, {
    method: "GET",
    headers: { "Authorization": `Bearer ${token}` }
  });

  const data = await res.json();
  console.log("📁 Propostas encontradas:", data);

  return data;
}

// ===============================
// 6. DETALHAR PROPOSTA POR ID
// ===============================
async function detalharProposta(id) {
  const token = await autenticar();
  if (!token) return;

  const res = await fetch(`${API_URL}/fgts/proposal/${id}`, {
    method: "GET",
    headers: { "Authorization": `Bearer ${token}` }
  });

  const data = await res.json();
  console.log("📄 Detalhes da proposta:", data);

  return data;
}

// ===============================
// 7. EXEMPLOS DE USO
// ===============================

// Consultar saldo
// consultarSaldoFGTS("12345678900");

// Buscar resultado da consulta
// buscarResultadoSaldo("12345678900");

// Criar proposta (payload de exemplo)
const exemploProposta = {
  documentNumber: "12345678900",
  name: "Fulano da Silva",
  birthDate: "1990-01-01",
  phone: "12999999999",
  email: "fulano@email.com",
  motherName: "Maria da Silva",

  pixKey: "12999999999",       // pode ser CPF, email ou telefone
  pixType: "phone",

  address: {
    zipCode: "12345000",
    street: "Rua A",
    number: "123",
    district: "Centro",
    city: "São Paulo",
    state: "SP"
  },

  periods: [2025, 2026, 2027], // parcelas antecipadas
  bank: "bms"                  // provedor da análise
};

// Criar proposta
// criarPropostaFGTS(exemploProposta);

// Listar propostas
// listarPropostas("12345678900");

// Detalhar proposta
// detalharProposta("ID_DA_PROPOSTA_AQUI");
