const Hyperswarm = require('hyperswarm')
const crypto = require('crypto')
const b4a = require('b4a')

const swarm = new Hyperswarm()
// Tópico comum derivado de uma string conhecida
const topic = crypto.createHash('sha256').update('kadepy-chat-v1').digest()

swarm.on('connection', (conn, info) => {
  const peerId = b4a.toString(conn.remotePublicKey, 'hex').substring(0, 6)
  console.log(`\n[${peerId}] conectado!`)
  
  // Conecta entrada/saída do terminal direto ao socket do peer
  process.stdin.pipe(conn).pipe(process.stdout)
  
  conn.on('error', e => console.error(`[${peerId}] erro:`, e.message))
  conn.on('close', () => console.log(`\n[${peerId}] saiu.`))
})

// Entra no tópico como cliente e servidor
swarm.join(topic)

console.log('Node.js peer entrou no tópico:', b4a.toString(topic, 'hex'))
console.log('Aguardando peers (digite algo para enviar)...')