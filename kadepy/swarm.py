import socket
import struct
import time
import threading
import json
import os
import subprocess
import sys
import atexit
from ._core import (
    create_swarm,
    set_callback,
    dht_ping,
    dht_find_node,
    dht_announce_peer,
    dht_get_peers,
    dump_routing_table,
    xor_distance
)

# Constants from protocol.h
MSG_PING = 0
MSG_PONG = 1
MSG_FIND_NODE = 2
MSG_FOUND_NODES = 3
MSG_ANNOUNCE_PEER = 4
MSG_GET_PEERS = 5
MSG_PEERS = 6

class Swarm:
    def __init__(self, port=0, bootstrap_nodes=None):
        """
        Inicializa o nó Swarm (KadePy).
        
        Args:
            port (int): Porta para vincular (Legacy DHT).
            bootstrap_nodes (list): Lista de tuplas (ip, port) para bootstrap inicial.
        """
        # Legacy DHT setup
        self.port = create_swarm(port)
        self.peers = {} 
        self._callback = None
        self.known_nodes = {}
        self.lock = threading.Lock()
        
        # Bridge Setup
        self._bridge_proc = None
        self._bridge_socket = None
        self._bridge_connected = False
        self._event_callbacks = {}
        self._bridge_thread = None
        
        # Register global callback for Legacy DHT
        set_callback(self._on_packet)
        
        if bootstrap_nodes:
            for ip, port in bootstrap_nodes:
                self.add_bootstrap_node(ip, port)
                
        # Register cleanup
        atexit.register(self.close)

    def _ensure_bridge(self):
        """Garante que o bridge Node.js esteja rodando e conectado."""
        if self._bridge_connected:
            return True

        bridge_script = os.path.join(os.path.dirname(__file__), 'js', 'bridge.js')
        if not os.path.exists(bridge_script):
            # Fallback for development
            bridge_script = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'bridge', 'hyperswarm_bridge.js'))
            
        if not os.path.exists(bridge_script):
            print(f"[Swarm] Erro: Script do bridge não encontrado.")
            return False

        # Verifica se node_modules existe
        node_modules = os.path.join(os.path.dirname(bridge_script), 'node_modules')
        if not os.path.exists(node_modules):
            print("[Swarm] Instalando dependências do Bridge (hyperswarm)...")
            try:
                subprocess.check_call(['npm', 'install'], cwd=os.path.dirname(bridge_script), shell=True)
            except Exception as e:
                print(f"[Swarm] Falha ao instalar dependências: {e}")
                return False

        # Inicia o processo do bridge
        if not self._bridge_proc:
            print("[Swarm] Iniciando processo Bridge Node.js...")
            env = os.environ.copy()
            env['KADEPY_BRIDGE_PORT'] = '5001'
            
            try:
                self._bridge_proc = subprocess.Popen(
                    ['node', os.path.basename(bridge_script)],
                    cwd=os.path.dirname(bridge_script),
                    env=env,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    shell=True if sys.platform == 'win32' else False
                )
                time.sleep(2)
                
                if self._bridge_proc.poll() is not None:
                    out, err = self._bridge_proc.communicate()
                    print(f"[Swarm] Bridge falhou ao iniciar:\n{err}")
                    self._bridge_proc = None
                    return False
            except Exception as e:
                print(f"[Swarm] Erro ao iniciar subprocesso: {e}")
                return False

        # Conecta via TCP
        try:
            self._bridge_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._bridge_socket.connect(('127.0.0.1', 5001))
            self._bridge_connected = True
            
            self._bridge_thread = threading.Thread(target=self._bridge_reader_loop, daemon=True)
            self._bridge_thread.start()
            
            print("[Swarm] Conectado ao Bridge com sucesso.")
            return True
        except Exception as e:
            print(f"[Swarm] Falha ao conectar TCP ao bridge: {e}")
            self.close()
            return False

    def _bridge_reader_loop(self):
        buffer = ""
        while self._bridge_connected and self._bridge_socket:
            try:
                data = self._bridge_socket.recv(4096)
                if not data: break
                buffer += data.decode('utf-8')
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    if line.strip():
                        try:
                            msg = json.loads(line)
                            self._handle_bridge_message(msg)
                        except: pass
            except: break
        self._bridge_connected = False

    def _handle_bridge_message(self, msg):
        event = msg.get('event')
        if event == 'data':
            if 'data' in self._event_callbacks:
                import base64
                try:
                    raw = base64.b64decode(msg.get('data'))
                    try:
                        decoded = raw.decode('utf-8')
                        self._event_callbacks['data'](decoded)
                    except:
                        self._event_callbacks['data'](raw)
                except: pass
        elif event == 'peer_connected':
             print(f"[Swarm] Novo peer conectado: {msg.get('peerId')}")

    def on(self, event, callback):
        self._event_callbacks[event] = callback

    def join(self, topic):
        if not self._ensure_bridge(): return
        if isinstance(topic, bytes): topic = topic.hex()
        msg = {"op": "join", "topic": topic}
        self._send_bridge(msg)

    def leave(self, topic):
        if isinstance(topic, bytes): topic = topic.hex()
        msg = {"op": "leave", "topic": topic}
        self._send_bridge(msg)

    def send(self, data):
        if not self._ensure_bridge(): return
        if isinstance(data, bytes):
            import base64
            b64 = base64.b64encode(data).decode('ascii')
            msg = {"op": "send_blob", "data": b64}
        else:
            msg = {"op": "send", "data": str(data)}
        self._send_bridge(msg)

    def _send_bridge(self, msg):
        if self._bridge_connected and self._bridge_socket:
            try:
                data = json.dumps(msg) + '\n'
                self._bridge_socket.sendall(data.encode('utf-8'))
            except: self._bridge_connected = False

    def close(self):
        self._bridge_connected = False
        if self._bridge_socket:
            try: self._bridge_socket.close()
            except: pass
        if self._bridge_proc:
            try: self._bridge_proc.terminate()
            except: pass
            self._bridge_proc = None

    def _on_packet(self, sender_id, msg_type, ip, port, payload, signature=None):
        pass

    def add_bootstrap_node(self, ip, port):
        pass

    def run(self):
        while True:
            time.sleep(1)

