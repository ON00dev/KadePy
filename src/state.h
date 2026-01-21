#ifndef STATE_H
#define STATE_H

#include <Python.h>

// Estrutura para manter o estado global se mudarmos para multi-phase init no futuro
typedef struct {
    int initialized;
} PyswarmState;

#endif
