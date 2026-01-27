const Hyperswarm = require('hyperswarm')
const net = require('net')
const crypto = require('crypto')
const readline = require('readline')

const swarm = new Hyperswarm()
const streams = new Map() // id -> stream

// Internal TCP Server for piping streams to Python
const server = net.createServer(clientSocket => {
  let buffer = ''
  
  const onData = (data) => {
    buffer += data.toString()
    const idx = buffer.indexOf('\n')
    if (idx !== -1) {
      const id = buffer.slice(0, idx).trim()
      clientSocket.removeListener('data', onData)
      
      // Push back any remaining data if needed, but usually handshake is first
      const remaining = buffer.slice(idx + 1)
      if (remaining.length > 0) clientSocket.unshift(Buffer.from(remaining))
      
      const stream = streams.get(id)
      if (stream) {
          // Pipe streams
          stream.pipe(clientSocket).pipe(stream)
          
          // Cleanup
          stream.on('close', () => clientSocket.destroy())
          stream.on('error', () => clientSocket.destroy())
          
          clientSocket.on('close', () => stream.destroy())
          clientSocket.on('error', () => stream.destroy())
      } else {
          clientSocket.end()
      }
    }
  }
  
  clientSocket.on('data', onData)
})

server.listen(0, '127.0.0.1', () => {
  const address = server.address()
  console.log(JSON.stringify({ event: 'ready', port: address.port }))
})

// Input handling
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  terminal: false
})

rl.on('line', (line) => {
  if (!line) return
  try {
    const msg = JSON.parse(line)
    handleMessage(msg)
  } catch (err) {
    // console.error('Invalid JSON:', err)
  }
})

async function handleMessage(msg) {
    const { id, method, args } = msg
    try {
        if (method === 'join') {
            const topic = Buffer.from(args.topic, 'hex')
            const discovery = swarm.join(topic, args.options)
            await discovery.flushed()
            reply(id, { status: 'flushed' })
        } else if (method === 'leave') {
            const topic = Buffer.from(args.topic, 'hex')
            await swarm.leave(topic)
            reply(id, { status: 'left' })
        } else if (method === 'getinfo') {
            const addr = swarm.dht ? swarm.dht.address() : null
            reply(id, { dhtPort: addr ? addr.port : null })
        } else if (method === 'destroy') {
            await swarm.destroy()
            reply(id, { status: 'destroyed' })
            process.exit(0)
        } else {
            reply(id, { error: 'Unknown method' })
        }
    } catch (err) {
        reply(id, { error: err.message })
    }
}

function reply(id, result) {
    if (id) console.log(JSON.stringify({ id, result }))
}

swarm.on('connection', (conn, info) => {
    const id = crypto.randomBytes(16).toString('hex')
    streams.set(id, conn)
    
    console.log(JSON.stringify({
        event: 'connection',
        peer: info.publicKey.toString('hex'),
        stream_id: id,
        client: info.client
    }))
    
    conn.on('close', () => {
        streams.delete(id)
        console.log(JSON.stringify({
            event: 'disconnection',
            stream_id: id
        }))
    })
    
    conn.on('error', (err) => {
        // console.error('Connection error:', err)
        streams.delete(id)
    })
})
