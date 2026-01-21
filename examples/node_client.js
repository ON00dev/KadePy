const { spawn } = require('child_process');
const readline = require('readline');
const path = require('path');

// Path to Python interpreter and IPC script
const pythonPath = "python"; // Assume python is in PATH

console.log(`Starting KadePy IPC Bridge: ${pythonPath} -m kadepy.ipc`);

// Run as a module to fix relative imports
const child = spawn(pythonPath, ['-m', 'kadepy.ipc']);

child.stderr.on('data', (data) => {
    console.error(`[KadePy Log]: ${data.toString().trim()}`);
});

child.on('close', (code) => {
    console.log(`KadePy process exited with code ${code}`);
});

const rl = readline.createInterface({
    input: child.stdout,
    terminal: false
});

rl.on('line', (line) => {
    try {
        const msg = JSON.parse(line);
        console.log('[Received from Python]:', msg);
    } catch (e) {
        console.error('Failed to parse JSON:', line);
    }
});

// Helper to send JSON-RPC
function sendRequest(method, params, id) {
    const req = {
        jsonrpc: "2.0",
        method: method,
        params: params,
        id: id || Date.now()
    };
    const str = JSON.stringify(req) + "\n";
    child.stdin.write(str);
    console.log('[Sent to Python]:', req);
}

// Example Interaction Sequence
setTimeout(() => {
    // 1. Check status
    sendRequest("start", { port: 9000 }, 1);
}, 1000);

setTimeout(() => {
    // 2. Send a Ping (example IP 127.0.0.1:8000)
    // Note: In a real scenario, you'd know who to ping.
    sendRequest("ping", { ip: "127.0.0.1", port: 8000 }, 2);
}, 2000);

setTimeout(() => {
    // 3. Find Node (random target)
    const target = "a" * 64; // Invalid hex but for example
    sendRequest("find_node", { 
        ip: "127.0.0.1", 
        port: 8000, 
        target_id: "0000000000000000000000000000000000000000000000000000000000000001" 
    }, 3);
}, 3000);

// Keep alive for a bit then exit
setTimeout(() => {
    console.log("Closing...");
    child.kill();
    process.exit(0);
}, 5000);
