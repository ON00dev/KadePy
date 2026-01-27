const Hyperswarm = require('hyperswarm')
const crypto = require('crypto')
const fs = require('fs')
const path = require('path')

const swarm = new Hyperswarm()
const topic = crypto.createHash('sha256').update('kadepy-native-test').digest()

swarm.on('update', () => {
    // Info about discovery
})

swarm.join(topic, { announce: true, lookup: true })

swarm.on('connection', (socket, info) => {
  console.log('[Node] New connection!')
  console.log('[Node] Remote Peer Public Key:', info.publicKey.toString('hex'))
  
  socket.write('Hello from Node.js!')
  
  socket.on('data', data => {
    console.log('[Node] Received:', data.toString())
  })
  
  socket.on('error', err => {
      console.error('[Node] Socket Error:', err.message)
  })
})

swarm.dht.ready().then(() => {
    const address = swarm.dht.address()
    const port = address.port
    console.log('[Node] Swarm listening on port:', port)
    console.log('[Node] Joining topic:', topic.toString('hex'))
    fs.writeFileSync(path.join(__dirname, 'node_port.txt'), port.toString())
})

// Keep alive
setInterval(() => {}, 1000)
