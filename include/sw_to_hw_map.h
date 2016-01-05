#ifndef _SW_TO_HW_MAP_H
#define _SW_TO_HW_MAP_H

#include "hwg.h"
#include "bg_impl.h"
#include "priority_list.h"

typedef struct {
    unsigned int swId;
    unsigned int hwId;
    bool fixed;
} sw2hw_map_entry_t;

typedef struct {
    char hwGraphName[HWG_MAX_STRING_LENGTH];
    char swGraphName[bg_MAX_STRING_LENGTH];

    hw_graph_t *hwGraph;
    bg_graph_t *swGraph;

    priority_list_t *assignments;
} sw2hw_map_t;

typedef enum {
    SW2HW_ERR_NONE = 0,
    SW2HW_ERR_UNKNOWN,
    SW2HW_ERR_DUPLICATE_ASSIGNMENT,
    SW2HW_ERR_NOMEM
} sw2hw_error;

sw2hw_error sw2hw_map_init(sw2hw_map_t *map, const char *hwGraphName, const char *swGraphName);
void sw2hw_map_destroy(sw2hw_map_t *map);

sw2hw_map_entry_t *sw2hw_map_get_assignment(sw2hw_map_t *map, const unsigned int swId);
sw2hw_error sw2hw_map_assign(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId, const int prio, const bool fixed);

/*Match SOURCES & SINKS in HW to INPUTS & OUTPUTS in SW by name*/
sw2hw_error sw2hw_premap(sw2hw_map_t *map);
/*All SW entities will be assigned to one and only one HW entity*/
sw2hw_error sw2hw_map(sw2hw_map_t *map);
/*Try to find better assignments*/
sw2hw_error sw2hw_map_refine(sw2hw_map_t *map);

#endif
