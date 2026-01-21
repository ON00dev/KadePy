import sys
import json
import logging
import threading
from kadepy.swarm import Swarm

# Configure logging to stderr to avoid polluting stdout (used for IPC)
logging.basicConfig(stream=sys.stderr, level=logging.INFO)
logger = logging.getLogger("KadePy-IPC")

class IPCBridge:
    def __init__(self):
        self.swarm = Swarm()
        self.running = True
        
        # Override the swarm callback to send events to stdout
        self.swarm.set_callback(self.on_dht_event)

    def on_dht_event(self, event_type, data):
        """Callback from DHT C-extension, forward to IPC stdout"""
        message = {
            "jsonrpc": "2.0",
            "method": "dht_event",
            "params": {
                "type": event_type,
                "data": data
            }
        }
        self.send_response(message)

    def send_response(self, msg):
        """Send JSON line to stdout"""
        try:
            sys.stdout.write(json.dumps(msg) + "\n")
            sys.stdout.flush()
        except Exception as e:
            logger.error(f"Failed to write to stdout: {e}")

    def handle_request(self, req):
        if "method" not in req:
            return {"error": "Missing method"}

        method = req["method"]
        params = req.get("params", {})
        msg_id = req.get("id", None)

        try:
            result = None
            if method == "ping":
                # params: ip, port
                # Swarm.bootstrap calls dht_ping under the hood, but Swarm has no direct ping method exposed 
                # except via bootstrap which adds to known nodes.
                # Or we can use dht_ping directly if we import it, but better to use Swarm methods.
                # Let's add a ping helper to Swarm or use bootstrap.
                self.swarm.bootstrap(params["ip"], params["port"])
                result = "Ping sent"
            elif method == "find_node":
                # params: ip, port, target_id (hex)
                # Swarm has find_node_iterative, but that's a high-level op.
                # If we want low-level find_node packet, Swarm doesn't expose it directly as method.
                # But we can import dht_find_node from _core.
                # However, the user probably wants the iterative lookup?
                # "find_node" in RPC usually implies sending a FIND_NODE packet.
                # But "find_node_iterative" is more useful.
                # Let's map "find_node" to the iterative lookup if target_id is provided, 
                # OR if ip/port provided, send a single packet.
                # The node_client.js sends ip/port/target_id, so it looks like a single packet request.
                
                target = bytes.fromhex(params["target_id"])
                
                # We need to access the low-level functions or expose them in Swarm
                from ._core import dht_find_node
                dht_find_node(params["ip"], params["port"], target)
                
                result = "FindNode sent"
            elif method == "announce":
                # params: ip, port, info_hash (hex), announced_port
                info_hash = bytes.fromhex(params["info_hash"])
                from ._core import dht_announce_peer
                dht_announce_peer(params["ip"], params["port"], info_hash, params["announced_port"])
                result = "Announce sent"
            elif method == "get_peers":
                # params: ip, port, info_hash (hex)
                info_hash = bytes.fromhex(params["info_hash"])
                from ._core import dht_get_peers
                dht_get_peers(params["ip"], params["port"], info_hash)
                result = "GetPeers sent"
            elif method == "start":
                # params: port (optional)
                port = params.get("port", 8000)
                # In a real app we might start a listener here if Swarm wasn't already started
                # But Swarm starts in __init__ currently.
                # We could re-bind or just return info.
                
                # Swarm doesn't store local ID in python object, it's in C. 
                # We can't easily get it without C extension modification or reading stdout logs.
                # For now, return a placeholder ID.
                result = {"status": "running", "local_id": "00"*32} 
            else:
                return {"jsonrpc": "2.0", "error": {"code": -32601, "message": "Method not found"}, "id": msg_id}

            return {"jsonrpc": "2.0", "result": result, "id": msg_id}

        except Exception as e:
            logger.exception("Error handling request")
            return {"jsonrpc": "2.0", "error": {"code": -32000, "message": str(e)}, "id": msg_id}

    def run(self):
        logger.info("IPC Bridge started. Waiting for JSON-RPC on stdin...")
        while self.running:
            try:
                line = sys.stdin.readline()
                if not line:
                    break
                
                line = line.strip()
                if not line:
                    continue

                try:
                    req = json.loads(line)
                    resp = self.handle_request(req)
                    if resp:
                        self.send_response(resp)
                except json.JSONDecodeError:
                    self.send_response({"jsonrpc": "2.0", "error": {"code": -32700, "message": "Parse error"}, "id": None})
            
            except KeyboardInterrupt:
                break
            except Exception as e:
                logger.error(f"IPC Loop Error: {e}")
                break

if __name__ == "__main__":
    bridge = IPCBridge()
    bridge.run()
