/**
 * stm.c - pequeño motor de maquina de estados donde los eventos son los
 *         del selector.c
 */
#include <stdlib.h>
#include "stm.h"
#include "logging/logger.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

void
stm_init(struct state_machine *stm) {
    // verificamos que los estados son correlativos, y que están bien asignados.
    for(unsigned i = 0 ; i <= stm->max_state; i++) {
        if(i != stm->states[i].state) {
            logf(LOG_FATAL, "State don't match: %d != %d", i, stm->states[i].state);
            abort();
        }
    }

    if(stm->initial < stm->max_state) {
        stm->current = NULL;
    } else {
        logf(LOG_FATAL, "Invalid initial state: %d", stm->initial);
        abort();
    }
}

// Maneja el caso en el que se llama a la maquina de estados por primera vez
// Ejecuta la funcion de entrada del estado inicial
inline static void
handle_first(struct state_machine *stm, struct selector_key *key) {
    if(stm->current == NULL) {
        stm->current = stm->states + stm->initial;
        if(NULL != stm->current->on_arrival) {
            stm->current->on_arrival(stm->current->state, key);
        }
    }
}


//Revisa si vengo de retornar el proximo estado o si fallo/se quedo ahí
inline static
void jump(struct state_machine *stm, unsigned next, struct selector_key *key) {

    if(next > stm->max_state) {
        logf(LOG_FATAL, "Invalid state: %d", next);
        abort();
    }
    if(stm->current != stm->states + next) {
        // Como se va a cambiar de estado, se ejecuta la funcion de salida del estado actual
        if(stm->current != NULL && stm->current->on_departure != NULL) {
            stm->current->on_departure(stm->current->state, key);
        }
        logf(LOG_DEBUG, "State change %d -> %d", stm->current->state, next);
        stm->current = stm->states + next;

        // Una vez que se cambio de estado, se ejecuta la funcion de entrada del nuevo estado
        if(NULL != stm->current->on_arrival) {
            stm->current->on_arrival(stm->current->state, key);
        }
    }
}

unsigned
stm_handler_read(struct state_machine *stm, struct selector_key *key) {
    // Comprueba si es la primera vez que se llama a la maquina de estados
    handle_first(stm, key);
    // Si no hay una funcion de lectura definida para el estado actual, aborta
    if(stm->current->on_read_ready == 0) {
        logf(LOG_FATAL, "Current state %d does not support read", stm->current->state);
        abort();
    }
    // Ejecuta la funcion de lectura del estado actual
    const unsigned int ret = stm->current->on_read_ready(key);
    // El resultado de la funcion de lectura es el proximo estado, el cual puede ser el mismo (se mantiene en el mismo estado)
    // o puede ser otro (en cuyo caso se hara el "jump" al nuevo estado)
    jump(stm, ret, key);

    return ret;
}

unsigned
stm_handler_write(struct state_machine *stm, struct selector_key *key) {
    // Comprueba si es la primera vez que se llama a la maquina de estados
    handle_first(stm, key);
    // Si no hay una funcion de escritura definida para el estado actual, aborta
    if(stm->current->on_write_ready == 0) {
        logf(LOG_FATAL, "Current state %d does not support write", stm->current->state);
        abort();
    }
    // Ejecuta la funcion de escritura del estado actual
    const unsigned int ret = stm->current->on_write_ready(key);
    // El resultado de la funcion de escritura es el proximo estado, el cual puede ser el mismo (se mantiene en el mismo estado)
    // o puede ser otro (en cuyo caso se hara el "jump" al nuevo estado)
    jump(stm, ret, key);

    return ret;
}

unsigned
stm_handler_block(struct state_machine *stm, struct selector_key *key) {
    handle_first(stm, key);
    if(stm->current->on_block_ready == 0) {
        logf(LOG_FATAL, "Current state %d does not support block", stm->current->state);
        abort();
    }
    const unsigned int ret = stm->current->on_block_ready(key);
    jump(stm, ret, key);

    return ret;
}

void
stm_handler_close(struct state_machine *stm, struct selector_key *key) {
    if(stm->current != NULL && stm->current->on_departure != NULL) {
        // Solo en el caso de que se haya utilizado la maquina de estados al menos una vez y
        // que el estado actual tenga una funcion de salida definida, se ejecuta dicha funcion
        stm->current->on_departure(stm->current->state, key);
    }
}

unsigned
stm_state(struct state_machine *stm) {
    // Devuelve el estado actual de la maquina de estados
    // Si no se utilizo la maquina de estados al menos una vez, devuelve el estado inicial
    unsigned ret = stm->initial;
    if(stm->current != NULL) {
        ret= stm->current->state;
    }
    return ret;
}
