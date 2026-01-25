#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "hyperswarm_core.h"

// --------------------------------------------------------------------------
// Python Object: HyperswarmNode
// --------------------------------------------------------------------------

typedef struct {
    PyObject_HEAD
    HyperswarmState* state; // Pointer to C-internal state
} HyperswarmNode;

static void HyperswarmNode_dealloc(HyperswarmNode* self) {
    if (self->state) {
        hyperswarm_destroy(self->state);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* HyperswarmNode_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    HyperswarmNode* self;
    self = (HyperswarmNode*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->state = NULL;
    }
    return (PyObject*)self;
}

static int HyperswarmNode_init(HyperswarmNode* self, PyObject* args, PyObject* kwds) {
    // Initialize the C state
    self->state = hyperswarm_create();
    if (!self->state) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create Hyperswarm state");
        return -1;
    }
    return 0;
}

// --------------------------------------------------------------------------
// Methods
// --------------------------------------------------------------------------

static PyObject* HyperswarmNode_init_bootstrap_node(HyperswarmNode* self, PyObject* args) {
    int port;
    int isolated_mode;
    if (!PyArg_ParseTuple(args, "ii", &port, &isolated_mode)) return NULL;
    
    if (hyperswarm_init_bootstrap_node(self->state, port, isolated_mode) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to initialize bootstrap node");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject* HyperswarmNode_join(HyperswarmNode* self, PyObject* args) {
    const char* topic_hex;
    if (!PyArg_ParseTuple(args, "s", &topic_hex)) {
        return NULL;
    }
    
    // In a real implementation, this would start the DHT lookup / holepunching
    hyperswarm_join(self->state, topic_hex);
    
    Py_RETURN_NONE;
}

static PyObject* HyperswarmNode_leave(HyperswarmNode* self, PyObject* args) {
    const char* topic_hex;
    if (!PyArg_ParseTuple(args, "s", &topic_hex)) {
        return NULL;
    }
    
    hyperswarm_leave(self->state, topic_hex);
    
    Py_RETURN_NONE;
}

static PyObject* HyperswarmNode_poll(HyperswarmNode* self, PyObject* args) {
    // Non-blocking poll for events (connections, data)
    // Returns a list of events or None
    hyperswarm_poll(self->state);
    Py_RETURN_NONE;
}

static PyObject* HyperswarmNode_get_port(HyperswarmNode* self, PyObject* args) {
    int port = hyperswarm_get_port(self->state);
    return PyLong_FromLong(port);
}

static PyObject* HyperswarmNode_send_debug(HyperswarmNode* self, PyObject* args) {
    const char* ip;
    int port;
    const char* msg;
    if (!PyArg_ParseTuple(args, "sis", &ip, &port, &msg)) return NULL;
    hyperswarm_send_debug(self->state, ip, port, msg);
    Py_RETURN_NONE;
}

static PyObject* HyperswarmNode_add_peer(HyperswarmNode* self, PyObject* args) {
    const char* ip;
    int port;
    const char* id_hex = NULL; // Optional
    if (!PyArg_ParseTuple(args, "si|s", &ip, &port, &id_hex)) return NULL;
    hyperswarm_add_peer(self->state, ip, port, id_hex);
    Py_RETURN_NONE;
}

static PyObject* HyperswarmNode_broadcast(HyperswarmNode* self, PyObject* args) {
    const char* msg;
    if (!PyArg_ParseTuple(args, "s", &msg)) return NULL;
    hyperswarm_broadcast(self->state, msg);
    Py_RETURN_NONE;
}

static PyObject* HyperswarmNode_get_connected_peer(HyperswarmNode* self, PyObject* args) {
    if (!self->state) Py_RETURN_NONE;
    
    for (int i=0; i<MAX_PEERS; i++) {
        if (self->state->peers[i].active && self->state->peers[i].hs_state == HS_STATE_ESTABLISHED) {
             return Py_BuildValue("(si)", self->state->peers[i].ip, self->state->peers[i].port);
        }
    }
    Py_RETURN_NONE;
}

static PyMethodDef HyperswarmNode_methods[] = {
    {"init_bootstrap_node", (PyCFunction)HyperswarmNode_init_bootstrap_node, METH_VARARGS, "Initialize as Bootstrap Node (Fixed Port, Isolated)"},
    {"join", (PyCFunction)HyperswarmNode_join, METH_VARARGS, "Join a topic"},
    {"leave", (PyCFunction)HyperswarmNode_leave, METH_VARARGS, "Leave a topic"},
    {"poll", (PyCFunction)HyperswarmNode_poll, METH_NOARGS, "Poll for network events"},
    {"get_port", (PyCFunction)HyperswarmNode_get_port, METH_NOARGS, "Get bound local port"},
    {"send_debug", (PyCFunction)HyperswarmNode_send_debug, METH_VARARGS, "Send debug packet"},
    {"add_peer", (PyCFunction)HyperswarmNode_add_peer, METH_VARARGS, "Add peer to routing table manually"},
    {"broadcast", (PyCFunction)HyperswarmNode_broadcast, METH_VARARGS, "Broadcast message to all connected peers"},
    {"get_connected_peer", (PyCFunction)HyperswarmNode_get_connected_peer, METH_NOARGS, "Get currently connected peer (IP, Port)"},
    {NULL}  /* Sentinel */
};

static PyTypeObject HyperswarmNodeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_hyperswarm.HyperswarmNode",
    .tp_doc = "Native Hyperswarm Node",
    .tp_basicsize = sizeof(HyperswarmNode),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = HyperswarmNode_new,
    .tp_init = (initproc)HyperswarmNode_init,
    .tp_dealloc = (destructor)HyperswarmNode_dealloc,
    .tp_methods = HyperswarmNode_methods,
};

// --------------------------------------------------------------------------
// Module Init
// --------------------------------------------------------------------------

static PyModuleDef hyperswarm_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_hyperswarm",
    .m_doc = "Native C implementation of Hyperswarm protocol",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit__hyperswarm(void) {
    PyObject* m;
    if (PyType_Ready(&HyperswarmNodeType) < 0)
        return NULL;

    m = PyModule_Create(&hyperswarm_module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&HyperswarmNodeType);
    if (PyModule_AddObject(m, "HyperswarmNode", (PyObject*)&HyperswarmNodeType) < 0) {
        Py_DECREF(&HyperswarmNodeType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
