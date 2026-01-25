const Hyperswarm = require('hyperswarm')
const crypto = require('crypto')
const b4a = require('b4a')

const swarm = new Hyperswarm()
const topic = crypto.createHash('sha256').update('kadepy-interop-test').digest()

console.log('Node.js Peer joining topic:', b4a.toString(topic, 'hex'))

swarm.join(topic)

swarm.on('connection', (conn, info) => {
  console.log('New connection from peer!')
  conn.write('Hello from Node.js!')
  conn.on('data', data => {
    console.log('Received from Python:', b4a.toString(data))
    if (b4a.toString(data).includes('Hello from Python')) {
        console.log('SUCCESS: Communication verified.')
        process.exit(0)
    }
  })
})
