import socket
import struct
import time
import threading
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
    def __init__(self, port=0):
        """
        Initialize the Swarm node.
        :param port: Port to bind to (0 for random).
        """
        # create_swarm returns the bound port
        self.port = create_swarm(port)
        self.peers = {} # info_hash -> list of peers
        self._callback = None
        
        self.known_nodes = {} # (ip, port) -> last_seen_timestamp
        self.lock = threading.Lock()
        
        # Register global callback
        set_callback(self._on_packet)
        
    def _on_packet(self, sender_id, msg_type, ip, port, payload):
        """
        Internal callback from C extension.
        """
        # IP is already a string from C extension
        ip_str = ip
        
        # Update known nodes
        with self.lock:
            self.known_nodes[(ip_str, port)] = time.time()
        
        if msg_type == MSG_FOUND_NODES:
            # Payload is a list of dicts {'id': bytes, 'ip': str, 'port': int}
            if isinstance(payload, list):
                with self.lock:
                    for node in payload:
                        self.known_nodes[(node['ip'], node['port'])] = time.time()
        
        # print(f"[Swarm] Packet {msg_type} from {ip_str}:{port}")
        
        if self._callback:
            self._callback(sender_id, msg_type, ip_str, port, payload)

    def set_callback(self, callback):
        """
        Set a callback for incoming packets.
        Callback signature: (sender_id: bytes, msg_type: int, ip: str, port: int, payload: any)
        """
        self._callback = callback

    def bootstrap(self, ip, port):
        """
        Bootstrap the node by contacting a known node.
        """
        dht_ping(ip, port)
        # We also speculatively add it to known nodes
        with self.lock:
            self.known_nodes[(ip, port)] = time.time()

    def _get_closest_nodes(self, target_id, k=8):
        """
        Get k closest known nodes to target_id.
        Since we don't have the node IDs for all known_nodes (only IPs), 
        we rely on the C routing table for the "best" nodes, but we can't query it.
        
        Workaround: We just return all known nodes because we are lazy 
        and maintaining IDs for all nodes in Python requires parsing all packets.
        
        Ideally, we should store ID in known_nodes too.
        """
        # For now, return all known nodes up to k (random/recent)
        with self.lock:
            nodes = list(self.known_nodes.keys())
        return nodes[:k]

    def find_node_iterative(self, target_id, timeout=2.0):
        """
        Perform an iterative lookup for target_id.
        Returns a list of (ip, port) of closest nodes found.
        """
        # Start by asking our known nodes
        start_time = time.time()
        queried = set()
        
        # Simple implementation: ask everyone we know, collecting more nodes
        # In a real implementation, we would sort by XOR distance.
        
        while time.time() - start_time < timeout:
            candidates = self._get_closest_nodes(target_id, k=20)
            to_query = [n for n in candidates if n not in queried]
            
            if not to_query:
                time.sleep(0.1)
                continue
            
            for ip, port in to_query:
                dht_find_node(ip, port, target_id)
                queried.add((ip, port))
            
            # Wait a bit for responses to populate known_nodes
            time.sleep(0.5)
            
        return self._get_closest_nodes(target_id)

    def announce(self, topic_hash, port=None):
        """
        Announce this node for a specific topic.
        :param topic_hash: 32-byte bytes object.
        :param port: Port to announce (defaults to bound port).
        """
        if port is None:
            port = self.port
            
        # 1. Find closest nodes to topic_hash
        closest_nodes = self.find_node_iterative(topic_hash)
        
        # 2. Send announce_peer to them
        for ip, target_port in closest_nodes:
            dht_announce_peer(ip, target_port, topic_hash, port)

    def get_peers(self, topic_hash, timeout=2.0):
        """
        Find peers for a topic.
        """
        # 1. Find closest nodes (they might store the peers)
        closest_nodes = self.find_node_iterative(topic_hash, timeout=timeout/2)
        
        # 2. Ask them for peers
        for ip, port in closest_nodes:
            dht_get_peers(ip, port, topic_hash)
            
        # 3. Wait for MSG_PEERS in callback (handled by user callback)
        # Here we just trigger the requests.

    def announce_to(self, target_ip, target_port, topic_hash, port=None):
        """
        Directly announce to a specific node.
        """
        if port is None:
            port = self.port
        dht_announce_peer(target_ip, target_port, topic_hash, port)

    def lookup_from(self, target_ip, target_port, topic_hash):
        """
        Directly ask a node for peers.
        """
        dht_get_peers(target_ip, target_port, topic_hash)

    def ping(self, ip, port):
        dht_ping(ip, port)

    def dump_table(self):
        dump_routing_table()
