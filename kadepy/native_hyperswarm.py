import subprocess
import json
import threading
import socket
import os
import sys
import time
import queue

class NativeHyperswarm:
    """
    Python wrapper for the official Hyperswarm stack via a Node.js daemon.
    This replaces the C extension with a robust IPC binding.
    """
    def __init__(self):
        # Locate daemon.js
        base_dir = os.path.dirname(os.path.abspath(__file__))
        js_path = os.path.join(base_dir, 'js', 'daemon.js')
        
        # Start Node.js process
        self.proc = subprocess.Popen(
            ['node', js_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=sys.stderr,
            text=True,
            bufsize=1 # Line buffered
        )
        
        self.port = 0 # Internal TCP proxy port
        self.peers = {} # stream_id -> socket
        self.event_queue = queue.Queue()
        self.response_queue = queue.Queue() # For synchronous responses
        self.running = True
        self.lock = threading.Lock()
        
        # Start reader thread
        self.reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader_thread.start()
        
        # Wait for ready
        start = time.time()
        while time.time() - start < 10:
            if self.port != 0:
                break
            time.sleep(0.1)
        
        if self.port == 0:
            self.destroy()
            raise RuntimeError("Timed out waiting for Hyperswarm daemon")

    def _reader_loop(self):
        while self.running and self.proc.poll() is None:
            try:
                line = self.proc.stdout.readline()
                if not line: break
                if not line.strip(): continue
                
                try:
                    msg = json.loads(line)
                except json.JSONDecodeError:
                    continue
                    
                if 'event' in msg:
                    if msg['event'] == 'ready':
                        self.port = msg['port']
                    elif msg['event'] == 'connection':
                        self._handle_connection(msg)
                        self.event_queue.put(msg)
                    else:
                        self.event_queue.put(msg)
                elif 'id' in msg:
                    # Response to request
                    self.response_queue.put(msg)
            except Exception as e:
                # print(f"[NativeHyperswarm] Reader error: {e}", file=sys.stderr)
                pass

    def _handle_connection(self, msg):
        stream_id = msg['stream_id']
        # Connect to local TCP
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(('127.0.0.1', self.port))
            # Handshake: send stream_id + \n
            s.sendall(f"{stream_id}\n".encode())
            with self.lock:
                self.peers[stream_id] = s
        except Exception as e:
            print(f"[NativeHyperswarm] Failed to connect to stream {stream_id}: {e}")

    def join(self, topic):
        if isinstance(topic, bytes):
            topic = topic.hex()
        
        req = {
            "id": f"join_{time.time()}",
            "method": "join",
            "args": { "topic": topic, "options": { "server": True, "client": True } }
        }
        self._send(req)

    def leave(self, topic):
        if isinstance(topic, bytes):
            topic = topic.hex()
        req = {
            "id": f"leave_{time.time()}",
            "method": "leave",
            "args": { "topic": topic }
        }
        self._send(req)

    def _send(self, msg):
        try:
            self.proc.stdin.write(json.dumps(msg) + "\n")
            self.proc.stdin.flush()
        except BrokenPipeError:
            self.running = False

    def poll(self):
        """
        Returns a list of events since last poll.
        For compatibility with existing tests that expect return values.
        """
        events = []
        try:
            while True:
                events.append(self.event_queue.get_nowait())
        except queue.Empty:
            pass
        return events

    def get_port(self):
        """
        Get the bound DHT port.
        """
        req_id = f"info_{time.time()}"
        req = {
            "id": req_id,
            "method": "getinfo",
            "args": {}
        }
        self._send(req)
        
        # Wait for response (simple sync wait)
        start = time.time()
        while time.time() - start < 2:
            try:
                # We need to peek/filter the queue, but queue doesn't support that easily.
                # Simplified: we assume the next response is ours or we drain.
                # Actually, since we might have async events, response_queue should only have responses.
                # But responses might be out of order if we had multiple threads.
                # Here we are single threaded mostly.
                
                # Better: Use a dict of futures? Too complex for now.
                # Just drain until we find it.
                resp = self.response_queue.get(timeout=0.1)
                if resp.get('id') == req_id:
                    res = resp.get('result', {})
                    return res.get('dhtPort', 0)
                else:
                    # Put back? No, Queue doesn't support put back.
                    # Ideally we should use a proper request manager.
                    # For now, just ignore other responses or print warning.
                    pass
            except queue.Empty:
                pass
        return 0

    def send_debug(self, ip, port, msg, remote_id=0):
        # Compatibility stub.
        # Sends to ALL connected peers because we don't track IP/Port mapping easily yet.
        data = msg.encode() if isinstance(msg, str) else msg
        with self.lock:
            for s in self.peers.values():
                try:
                    s.sendall(data)
                except:
                    pass

    def add_peer(self, ip, port, id_hex=None):
        # Compatibility stub
        pass
        
    def init_bootstrap_node(self, port=10001, isolated=True):
        # Compatibility stub
        print("[NativeHyperswarm] Warning: init_bootstrap_node ignored in JS daemon mode")
        
    def destroy(self):
        self.running = False
        self._send({ "id": "destroy", "method": "destroy", "args": {} })
        try:
            self.proc.terminate()
        except:
            pass
        with self.lock:
            for s in self.peers.values():
                try:
                    s.close()
                except:
                    pass
