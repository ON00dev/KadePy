#define PY_SSIZE_T_CLEAN
#include <Python.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
#endif

#include "state.h"
#include "dht/routing.h"
#include "dht/protocol.h"
#include "net/udp_reactor.h"

#define UNUSED(x) (void)(x)

// --- Global State ---
static PyObject *packet_callback = NULL;
static int g_has_callback = 0;

#include "dht/node.h" // Include node definition

#include "dht/storage.h"

// --- Helper Methods ---

void notify_python_packet(const uint8_t *sender_id, int type, uint32_t ip, uint16_t port, const uint8_t *payload, int payload_len, const uint8_t *signature) {
    // Optimization: Don't acquire GIL if no callback is registered
    if (!g_has_callback) return;

    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    if (packet_callback) {
        // Converter IP para string
        struct in_addr ip_addr;
        ip_addr.s_addr = htonl(ip);
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str));

        PyObject *py_id = PyBytes_FromStringAndSize((char*)sender_id, 32);
        PyObject *py_type = PyLong_FromLong(type);
        PyObject *py_ip = PyUnicode_FromString(ip_str);
        PyObject *py_port = PyLong_FromLong(port);
        PyObject *py_payload = Py_None;
        PyObject *py_signature = PyBytes_FromStringAndSize((char*)signature, 64);

        // Parse Payload based on Type
        if (type == MSG_PEERS) {
            if (payload_len >= sizeof(dht_peers_header_t)) {
                const dht_peers_header_t *header = (const dht_peers_header_t*)payload;
                int count = header->count;
                int expected_size = sizeof(dht_peers_header_t) + (count * sizeof(dht_peer_wire_t));
                
                if (payload_len >= expected_size) {
                    py_payload = PyList_New(count);
                    const uint8_t *ptr = payload + sizeof(dht_peers_header_t);
                    
                    for (int i = 0; i < count; i++) {
                        dht_peer_wire_t *wire = (dht_peer_wire_t*)ptr;
                        struct in_addr peer_addr;
                        peer_addr.s_addr = wire->ip;
                        char peer_ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &peer_addr, peer_ip_str, sizeof(peer_ip_str));
                        
                        PyObject *peer_dict = PyDict_New();
                        PyDict_SetItemString(peer_dict, "ip", PyUnicode_FromString(peer_ip_str));
                        PyDict_SetItemString(peer_dict, "port", PyLong_FromLong(ntohs(wire->port)));
                        
                        PyList_SetItem(py_payload, i, peer_dict); // Steals reference
                        ptr += sizeof(dht_peer_wire_t);
                    }
                }
            }
        }
        else if (type == MSG_FOUND_NODES) {
            // MSG_FOUND_NODES format: count (1 byte) + nodes...
            // Note: dht_found_nodes_header_t in protocol.h has count as uint8_t.
            // But wait, let's check dht_found_nodes_header_t definition again.
            // It is just { uint8_t count; }.
            
            if (payload_len >= 1) {
                uint8_t count = payload[0];
                int expected_size = 1 + (count * sizeof(dht_node_wire_t));
                
                if (payload_len >= expected_size) {
                    py_payload = PyList_New(count);
                    const uint8_t *ptr = payload + 1;
                    
                    for (int i = 0; i < count; i++) {
                         dht_node_wire_t *wire = (dht_node_wire_t*)ptr;
                         struct in_addr node_addr;
                        node_addr.s_addr = wire->ip;
                        char node_ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &node_addr, node_ip_str, sizeof(node_ip_str));
                        
                        PyObject *node_dict = PyDict_New();
                        PyDict_SetItemString(node_dict, "id", PyBytes_FromStringAndSize((char*)wire->id, 32));
                        PyDict_SetItemString(node_dict, "ip", PyUnicode_FromString(node_ip_str));
                        PyDict_SetItemString(node_dict, "port", PyLong_FromLong(ntohs(wire->port)));
                        
                        PyList_SetItem(py_payload, i, node_dict);
                        ptr += sizeof(dht_node_wire_t);
                    }
                }
            }
        }
        
        if (py_payload == Py_None) {
            Py_INCREF(Py_None);
        }

        // Updated callback signature: (id, type, ip, port, payload, signature)
        PyObject *args = PyTuple_Pack(6, py_id, py_type, py_ip, py_port, py_payload, py_signature);
        
        PyObject *result = PyObject_CallObject(packet_callback, args);
        
        if (result == NULL) {
            PyErr_Print(); // Print error if callback failed
        } else {
            Py_DECREF(result);
        }

        Py_DECREF(args);
        Py_DECREF(py_id);
        Py_DECREF(py_type);
        Py_DECREF(py_ip);
        Py_DECREF(py_port);
        Py_DECREF(py_payload);
        Py_DECREF(py_signature);
    }

    PyGILState_Release(gstate);
}

static PyObject* py_set_callback(PyObject* self, PyObject* args) {
    UNUSED(self);
    PyObject *temp;
    if (!PyArg_ParseTuple(args, "O", &temp)) {
        return NULL;
    }

    if (!PyCallable_Check(temp)) {
        PyErr_SetString(PyExc_TypeError, "Parameter must be callable");
        return NULL;
    }

    Py_XINCREF(temp);
    Py_XDECREF(packet_callback);
    packet_callback = temp;
    g_has_callback = 1;

    Py_RETURN_NONE;
}

static PyObject* py_xor_distance(PyObject* self, PyObject* args) {
    UNUSED(self);
    const char *id1;
    Py_ssize_t len1;
    const char *id2;
    Py_ssize_t len2;

    if (!PyArg_ParseTuple(args, "y#y#", &id1, &len1, &id2, &len2)) {
        return NULL;
    }

    if (len1 != 32 || len2 != 32) {
        PyErr_SetString(PyExc_ValueError, "IDs must be 32 bytes long");
        return NULL;
    }

    uint8_t result[32];
    xor_distance((const uint8_t*)id1, (const uint8_t*)id2, result);

    return PyBytes_FromStringAndSize((char*)result, 32);
}

static PyObject* py_dht_ping(PyObject* self, PyObject* args) {
    UNUSED(self);
    const char *ip_str;
    int port;
    if (!PyArg_ParseTuple(args, "si", &ip_str, &port)) {
        return NULL;
    }
    
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        PyErr_SetString(PyExc_ValueError, "Invalid IP address");
        return NULL;
    }
    uint32_t ip = ntohl(addr.s_addr);
    
    dht_ping(ip, (uint16_t)port);
    Py_RETURN_NONE;
}

static PyObject* py_dht_find_node(PyObject* self, PyObject* args) {
    UNUSED(self);
    const char *ip_str;
    int port;
    const char *target_id;
    Py_ssize_t len;
    
    if (!PyArg_ParseTuple(args, "siy#", &ip_str, &port, &target_id, &len)) {
        return NULL;
    }
    
    if (len != 32) {
        PyErr_SetString(PyExc_ValueError, "Target ID must be 32 bytes");
        return NULL;
    }

    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        PyErr_SetString(PyExc_ValueError, "Invalid IP address");
        return NULL;
    }
    uint32_t ip = addr.s_addr;
    
    dht_find_node(ip, (uint16_t)port, (const uint8_t*)target_id);
    Py_RETURN_NONE;
}

static PyObject* py_dump_routing_table(PyObject* self, PyObject* args) {
    UNUSED(self);
    UNUSED(args);
    dht_dump_routing_table();
    Py_RETURN_NONE;
}

static PyObject* py_dht_announce_peer(PyObject* self, PyObject* args) {
    UNUSED(self);
    const char *ip_str;
    int port;
    const char *info_hash;
    Py_ssize_t len;
    int announced_port;
    
    if (!PyArg_ParseTuple(args, "siy#i", &ip_str, &port, &info_hash, &len, &announced_port)) {
        return NULL;
    }
    
    if (len != 32) {
        PyErr_SetString(PyExc_ValueError, "Info Hash must be 32 bytes");
        return NULL;
    }

    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        PyErr_SetString(PyExc_ValueError, "Invalid IP address");
        return NULL;
    }
    uint32_t ip = ntohl(addr.s_addr);
    
    dht_announce_peer(ip, (uint16_t)port, (const uint8_t*)info_hash, (uint16_t)announced_port);
    Py_RETURN_NONE;
}

static PyObject* py_dht_get_peers(PyObject* self, PyObject* args) {
    UNUSED(self);
    const char *ip_str;
    int port;
    const char *info_hash;
    Py_ssize_t len;
    
    if (!PyArg_ParseTuple(args, "siy#", &ip_str, &port, &info_hash, &len)) {
        return NULL;
    }
    
    if (len != 32) {
        PyErr_SetString(PyExc_ValueError, "Info Hash must be 32 bytes");
        return NULL;
    }

    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        PyErr_SetString(PyExc_ValueError, "Invalid IP address");
        return NULL;
    }
    uint32_t ip = ntohl(addr.s_addr);
    
    dht_get_peers(ip, (uint16_t)port, (const uint8_t*)info_hash);
    Py_RETURN_NONE;
}

static PyObject* py_send_packet(PyObject* self, PyObject* args) {
    UNUSED(self);
    const char *ip_str;
    int port;
    int type;
    const char *payload;
    Py_ssize_t len;
    
    if (!PyArg_ParseTuple(args, "siiy#", &ip_str, &port, &type, &payload, &len)) {
        return NULL;
    }
    
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        PyErr_SetString(PyExc_ValueError, "Invalid IP address");
        return NULL;
    }
    uint32_t ip = ntohl(addr.s_addr);
    
    dht_send_packet(ip, (uint16_t)port, type, (const uint8_t*)payload, (int)len);
    Py_RETURN_NONE;
}

static PyObject* py_storage_cleanup(PyObject* self, PyObject* args) {
    UNUSED(self);
    long long timeout_ms;
    if (!PyArg_ParseTuple(args, "L", &timeout_ms)) {
        return NULL;
    }
    
    storage_cleanup((uint64_t)timeout_ms);
    Py_RETURN_NONE;
}

// --- Módulo C ---

static PyObject* py_set_network_key(PyObject* self, PyObject* args) {
    UNUSED(self);
    const char *key;
    Py_ssize_t len;
    
    if (PyTuple_Size(args) == 0 || (PyTuple_Size(args) == 1 && PyTuple_GetItem(args, 0) == Py_None)) {
        dht_set_network_key(NULL);
        Py_RETURN_NONE;
    }
    
    if (!PyArg_ParseTuple(args, "y#", &key, &len)) {
        return NULL;
    }
    
    if (len != 32) {
        PyErr_SetString(PyExc_ValueError, "Network Key must be 32 bytes");
        return NULL;
    }
    
    dht_set_network_key((const uint8_t*)key);
    Py_RETURN_NONE;
}

static PyObject* py_create_swarm(PyObject* self, PyObject* args) {
    UNUSED(self);
    int port = 0; // 0 = Random port
    if (!PyArg_ParseTuple(args, "|i", &port)) {
        return NULL;
    }

    printf("[C] Creating Swarm on port %d...\n", port);
    
    // Inicializar DHT core
    dht_init();
    
    if (udp_reactor_start(&port) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to start UDP reactor");
        return NULL;
    }
    
    // Return the actual port bound
    return PyLong_FromLong(port);
}

static PyMethodDef PyswarmMethods[] = {
    {"create_swarm", py_create_swarm, METH_VARARGS, "Create a new Swarm instance."},
    {"xor_distance", py_xor_distance, METH_VARARGS, "Calculate XOR distance between two 32-byte IDs."},
    {"set_callback", py_set_callback, METH_VARARGS, "Set the packet callback function."},
    {"dht_ping", py_dht_ping, METH_VARARGS, "Send a PING packet to a node."},
    {"dht_find_node", py_dht_find_node, METH_VARARGS, "Send a FIND_NODE packet to a node."},
    {"dht_announce_peer", py_dht_announce_peer, METH_VARARGS, "Send ANNOUNCE_PEER to a node."},
    {"dht_get_peers", py_dht_get_peers, METH_VARARGS, "Send GET_PEERS to a node."},
    {"send_packet", py_send_packet, METH_VARARGS, "Send a generic packet to a node."},
    {"set_network_key", py_set_network_key, METH_VARARGS, "Set the network key (32 bytes) for encryption. Pass None to disable."},
    {"storage_cleanup", py_storage_cleanup, METH_VARARGS, "Remove peers not seen for X milliseconds."},
    {"dump_routing_table", py_dump_routing_table, METH_VARARGS, "Dump routing table to stdout."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef kadepymodule = {
    PyModuleDef_HEAD_INIT,
    "kadepy._core",   // nome do módulo
    "Core C extension for KadePy", // documentação
    -1,       // tamanho do estado global (-1 = keep in global vars, mas idealmente usaríamos struct)
    PyswarmMethods
};

PyMODINIT_FUNC PyInit__core(void) {
    return PyModule_Create(&kadepymodule);
}
