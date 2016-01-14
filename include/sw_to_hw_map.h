#ifndef _SW_TO_HW_MAP_H
#define _SW_TO_HW_MAP_H

#include "hwg.h"
#include "bg_impl.h"
#include "priority_list.h"

#define SW2HW_MAX_STRING_LENGTH HWG_MAX_STRING_LENGTH

typedef struct {
    unsigned int swId;
    unsigned int hwId;
    bool fixed;
} sw2hw_map_entry_t;

typedef struct {
    hw_graph_t *hwGraph;
    bg_graph_t *swGraph;

    priority_list_t *assignments;
} sw2hw_map_t;

typedef enum {
    SW2HW_ERR_NONE = 0,
    SW2HW_ERR_UNKNOWN,
    SW2HW_ERR_DUPLICATE_ASSIGNMENT,
    SW2HW_ERR_NOMEM,
    SW2HW_ERR_NOT_FOUND
} sw2hw_error;

typedef int (*costFuncPtr)(sw2hw_map_t *, const unsigned int, const unsigned int);

sw2hw_error sw2hw_map_init(sw2hw_map_t *map, const hw_graph_t *hwGraph, const bg_graph_t *swGraph);
void sw2hw_map_destroy(sw2hw_map_t *map);

sw2hw_map_entry_t *sw2hw_map_get_assignment(sw2hw_map_t *map, const unsigned int swId);
sw2hw_error sw2hw_map_assign(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId, const int prio, const bool fixed);

/*Match SOURCES & SINKS in HW to INPUTS & OUTPUTS in SW by name*/
sw2hw_error sw2hw_map_match(sw2hw_map_t *map);

/*Standard cost functions*/
int costByNeighbours(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId);
int costByExternalEdges(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId);
int costByAssignments(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId);

/*NOTE: The following functions need a cost function to be passed*/
/*For all current assignments priority list is updated and sum of costs returned*/
int sw2hw_map_calculate_costs(sw2hw_map_t *map, costFuncPtr cost);
/*All unassigned SW entities will be assigned to one and only one HW entity*/
sw2hw_error sw2hw_map_initial(sw2hw_map_t *map, costFuncPtr cost);
/*Try to find better targets for assigned sw entities*/
sw2hw_error sw2hw_map_refine(sw2hw_map_t *map, costFuncPtr cost);
/*Try to find better targets for groups of assinments*/
sw2hw_error sw2hw_map_regroup(sw2hw_map_t *map, costFuncPtr cost);

/*For a specific target id, create a subgraph from assignments*/
sw2hw_error sw2hw_map_create_subgraph(sw2hw_map_t *map, const unsigned int hwId, bg_graph_t *sub);
/*After mapping this function creates a graph containing one subgraph per target*/
sw2hw_error sw2hw_map_create_graph(sw2hw_map_t *map, bg_graph_t *graph);
/*This function transforms the original mapping to one with a sw toplvl and subgraphs and corresponding assignments*/
sw2hw_error sw2hw_map_transform(sw2hw_map_t *dest, sw2hw_map_t *src);
bool sw2hw_map_is_transformed(sw2hw_map_t *map);

#endif
