const dgram = require('dgram');
const server = dgram.createSocket('udp4');
const fs = require('fs');
const path = require('path');

server.on('error', (err) => {
  console.log(`server error:\n${err.stack}`);
  server.close();
});

server.on('message', (msg, rinfo) => {
  console.log(`[Sniffer] Received ${msg.length} bytes from ${rinfo.address}:${rinfo.port}`);
  console.log(`[Sniffer] Hex: ${msg.toString('hex')}`);
});

server.on('listening', () => {
  const address = server.address();
  console.log(`[Sniffer] listening ${address.address}:${address.port}`);
  // Write port to file
  fs.writeFileSync(path.join(__dirname, 'node_port.txt'), address.port.toString());
});

server.bind(0); // Random port
