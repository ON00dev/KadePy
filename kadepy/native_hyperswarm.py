from ._hyperswarm import HyperswarmNode as NativeNode

class NativeHyperswarm:
    """
    Python wrapper for the native C Hyperswarm extension.
    This eliminates the need for the Node.js bridge.
    """
    def __init__(self):
        self._node = NativeNode()
        
    def init_bootstrap_node(self, port=10001, isolated=True):
        """
        Initialize this node as a Bootstrap Node (Private Network Beacon).
        
        Args:
            port (int): Fixed port to bind to (default: 10001).
            isolated (bool): If True, starts with empty bootstrap list (Isolated Mode).
        """
        if hasattr(self._node, 'init_bootstrap_node'):
            self._node.init_bootstrap_node(port, int(isolated))
        else:
            raise NotImplementedError("Native extension does not support init_bootstrap_node")

    def join(self, topic):
        """
        Join a topic on the Hyperswarm network.
        Topic can be bytes or hex string.
        """
        if isinstance(topic, bytes):
            topic_hex = topic.hex()
        else:
            topic_hex = topic
        self._node.join(topic_hex)
        
    def leave(self, topic):
        """Leave a topic."""
        if isinstance(topic, bytes):
            topic_hex = topic.hex()
        else:
            topic_hex = topic
        self._node.leave(topic_hex)
        
    def poll(self):
        """Poll for events."""
        return self._node.poll()

    def get_port(self):
        """Get the bound local port."""
        return self._node.get_port()

    def send_debug(self, ip, port, msg):
        """Send a debug packet."""
        return self._node.send_debug(ip, port, msg)

    def add_peer(self, ip, port, id_hex=None):
        """Manually add a peer to the routing table."""
        if hasattr(self._node, 'add_peer'):
            if id_hex:
                return self._node.add_peer(ip, port, id_hex)
            else:
                return self._node.add_peer(ip, port)
        else:
            print("Warning: Native extension does not support add_peer yet.")
