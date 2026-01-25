const Hyperswarm = require('hyperswarm')
const net = require('net')
const b4a = require('b4a')
const crypto = require('crypto')

const swarm = new Hyperswarm()
const peers = new Map() // peerId -> connection

// Handle Hyperswarm connections
swarm.on('connection', (conn, info) => {
  const peerId = b4a.toString(conn.remotePublicKey, 'hex')
  peers.set(peerId, conn)
  
  // Notify Python about new peer
  sendToPython({ event: 'peer_connected', peerId: peerId })

  conn.on('data', data => {
    // Forward data to Python
    sendToPython({ 
      event: 'data', 
      peerId: peerId, 
      data: b4a.toString(data, 'base64') 
    })
  })

  conn.on('close', () => {
    peers.delete(peerId)
    sendToPython({ event: 'peer_disconnected', peerId: peerId })
  })

  conn.on('error', err => {
    // console.error(`[Bridge] Connection error with ${peerId}:`, err.message)
  })
})

// TCP Server for Python connection
const server = net.createServer(socket => {
  console.log('[Bridge] Python connected')
  
  // Buffer for incoming data handling (lines)
  let buffer = ''

  socket.on('data', chunk => {
    buffer += chunk.toString()
    const lines = buffer.split('\n')
    buffer = lines.pop()

    for (const line of lines) {
      if (!line.trim()) continue
      try {
        const msg = JSON.parse(line)
        handlePythonMessage(msg, socket)
      } catch (e) {
        console.error('[Bridge] Invalid JSON:', e.message)
      }
    }
  })

  socket.on('close', () => {
    console.log('[Bridge] Python disconnected. Exiting...')
    process.exit(0)
  })
})

// Helper to send JSON to Python
let pythonSocket = null
server.on('connection', sock => {
  pythonSocket = sock
})

function sendToPython(msg) {
  if (pythonSocket && !pythonSocket.destroyed) {
    pythonSocket.write(JSON.stringify(msg) + '\n')
  }
}

function handlePythonMessage(msg, socket) {
  switch (msg.op) {
    case 'join':
      try {
        const topic = b4a.from(msg.topic, 'hex')
        if (topic.length !== 32) {
             console.error('[Bridge] Invalid topic length:', topic.length)
             return
        }
        const opts = {
            announce: msg.announce !== false,
            lookup: msg.lookup !== false
        }
        swarm.join(topic, opts)
        console.log(`[Bridge] Joined topic: ${msg.topic} (announce=${opts.announce}, lookup=${opts.lookup})`)
      } catch (e) {
        console.error('[Bridge] Join error:', e)
      }
      break
      
    case 'leave':
      try {
        const topic = b4a.from(msg.topic, 'hex')
        swarm.leave(topic)
      } catch (e) {}
      break

    case 'send':
      // Broadcast or send to specific peer
      const data = b4a.from(msg.data, 'utf-8') // Expecting utf-8 string from Python for now
      for (const conn of peers.values()) {
        conn.write(data)
      }
      break
      
    case 'send_blob':
        // Send base64 data
        if (msg.data) {
            const blob = b4a.from(msg.data, 'base64')
            for (const conn of peers.values()) {
                conn.write(blob)
            }
        }
        break

    default:
      console.warn('[Bridge] Unknown op:', msg.op)
  }
}

const PORT = process.env.KADEPY_BRIDGE_PORT || 5000
server.listen(PORT, '127.0.0.1', () => {
  console.log(`[Bridge] Listening on 127.0.0.1:${PORT}`)
})
