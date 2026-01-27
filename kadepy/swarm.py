import threading
import time
import atexit
from .native_hyperswarm import NativeHyperswarm

class Swarm:
    def __init__(self, port=0, bootstrap_nodes=None):
        """
        Initialize the Swarm node using the official Hyperswarm stack (via NativeHyperswarm).
        
        Args:
            port (int): Port to bind (UDP). Note: In the current implementation, 
                        the port is assigned by the daemon, this argument is mostly ignored 
                        unless we pass it to the daemon config.
            bootstrap_nodes (list): List of tuples (ip, port). Ignored in official stack 
                                    (uses global DHT by default).
        """
        self._node = NativeHyperswarm()
        self._event_callbacks = {}
        self._running = True
        
        # Start the event polling loop
        self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
        self._poll_thread.start()
        
        atexit.register(self.close)

    def on(self, event, callback):
        """
        Register an event handler.
        Supported events: 'connection'
        """
        self._event_callbacks[event] = callback

    def join(self, topic, announce=True, lookup=True):
        """
        Join a topic in the swarm.
        
        Args:
            topic (str or bytes): 32-byte topic (hex string or bytes).
            announce (bool): Whether to announce presence.
            lookup (bool): Whether to look for peers.
        """
        if isinstance(topic, str):
            if len(topic) != 64:
                # Assuming hex string
                pass
            # Normalize to hex for internal use if needed, or keep as is.
            # NativeHyperswarm handles bytes or hex.
            pass
        
        # In official Hyperswarm, join implies both announce and lookup usually,
        # but we can control it via options if we expose them.
        # For now, simple join.
        self._node.join(topic)

    def leave(self, topic):
        """
        Leave a topic.
        """
        self._node.leave(topic)

    def send(self, data):
        """
        Broadcast data to all connected peers.
        """
        # This is a high-level helper not present in raw Hyperswarm usually (which is connection based),
        # but KadePy Swarm had it.
        # We can iterate over peers in NativeHyperswarm.
        self._node.send_debug(None, None, data)

    def close(self):
        """
        Destroy the swarm node.
        """
        self._running = False
        if self._node:
            self._node.destroy()
            self._node = None

    def _poll_loop(self):
        while self._running:
            try:
                events = self._node.poll()
                for event in events:
                    self._handle_event(event)
                time.sleep(0.01)
            except Exception as e:
                if self._running:
                    print(f"Error in poll loop: {e}")
                break

    def _handle_event(self, event):
        if 'event' not in event:
            return
            
        evt_type = event['event']
        
        if evt_type == 'connection':
            # Hyperswarm connection event
            # In KadePy, we might want to expose the socket or a wrapper
            # NativeHyperswarm already connects a local TCP socket for this stream.
            # We can retrieve it.
            stream_id = event.get('stream_id')
            peer_socket = self._node.peers.get(stream_id)
            
            if 'connection' in self._event_callbacks:
                self._event_callbacks['connection'](peer_socket, event.get('info', {}))
                
        elif evt_type == 'data':
            # If we implemented data sniffing in daemon
            if 'data' in self._event_callbacks:
                self._event_callbacks['data'](event.get('data'))

    def add_bootstrap_node(self, ip, port):
        pass

    def run(self):
        """
        Block main thread (optional).
        """
        while self._running:
            time.sleep(1)
