const UDX = require('udx-native')
const fs = require('fs')
const path = require('path')

const u = new UDX()
const socket = u.createSocket()

try {
    socket.bind(0)
} catch (e) {
    console.error(e)
}

// Wait for port file
const portPath = path.join(__dirname, 'node_port.txt')
const interval = setInterval(() => {
    if (fs.existsSync(portPath)) {
        clearInterval(interval)
        const port = parseInt(fs.readFileSync(portPath, 'utf-8'))
        console.log("Connecting to port", port)

        const stream = u.createStream(5) // My Stream ID = 5
        // Connect to sniffer (simulating a peer)
        // Note: udx-native might require handshake before sending data?
        // Or it sends SYN immediately.
        stream.connect(socket, 0x11223344, port, '127.0.0.1') // Remote Stream ID
        
        const longPayload = Buffer.alloc(100).fill('C')
        stream.write(longPayload)
        
        setTimeout(() => {
            console.log("Exiting spy")
            process.exit(0)
        }, 1000)
    }
}, 500)
