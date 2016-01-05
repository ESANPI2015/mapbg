#include "sw_to_hw_map.h"
#include "hwg_yaml.h"
#include "node_list.h"
#include "bg_graph.h"
#include <stdio.h>
#include <string.h>

/*For external edges the cost are higher than for neighbouring edges which have higher costs than internal edges*/
static int costByNeighbours(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId)
{
    int norm = 0;
    int cost = 0;
    unsigned int i,j,k;
    sw2hw_map_entry_t *assignment = NULL;
    hw_node_t *other;
    bg_node_t *swNode;
    hw_node_t *hwNode;

    bg_graph_find_node(map->swGraph, swId, &swNode);
    hwNode = hw_graph_get_node(map->hwGraph, hwId);

    for (i = 0; i < swNode->input_port_cnt; ++i)
        for (j = 0; j < swNode->input_ports[i]->num_edges; ++j)
        {
            if ((assignment = sw2hw_map_get_assignment(map, swNode->input_ports[i]->edges[j]->source_node->id)))
            {
                if (assignment->hwId == hwNode->id)
                {
                    cost += 1;
                } else {
                    /*External edge: Is it a neighbour?*/
                    other = hw_graph_get_node(map->hwGraph, assignment->hwId);
                    for (k = 0; k < hwNode->numPorts; ++k)
                    {
                        if (hwNode->ports[k].edge && ((hwNode->ports[k].edge->nodes[0] == other) || (hwNode->ports[k].edge->nodes[1] == other)))
                        {
                            cost += 2;
                            break;
                        }
                    }
                    if (k == hwNode->numPorts)
                        cost += 3;
                }
            }
            norm += 3;
        }

    for (i = 0; i < swNode->output_port_cnt; ++i)
        for (j = 0; j < swNode->output_ports[i]->num_edges; ++j)
        {
            if ((assignment = sw2hw_map_get_assignment(map, swNode->output_ports[i]->edges[j]->sink_node->id)))
            {
                if (assignment->hwId == hwNode->id)
                {
                    cost += 1;
                } else {
                    /*External edge: Is it a neighbour?*/
                    other = hw_graph_get_node(map->hwGraph, assignment->hwId);
                    for (k = 0; k < hwNode->numPorts; ++k)
                    {
                        if (hwNode->ports[k].edge && ((hwNode->ports[k].edge->nodes[0] == other) || (hwNode->ports[k].edge->nodes[1] == other)))
                        {
                            cost += 2;
                            break;
                        }
                    }
                    if (k == hwNode->numPorts)
                        cost += 3;
                }
            }
            norm += 3;
        }

    return cost * 100 / norm;
}

/*Internal vs. external edge count*/
/*static int costByConnectivity(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId)*/
/*{*/
    /*int norm = 0;*/
    /*int cost = 0;*/
    /*unsigned int i,j;*/
    /*sw2hw_map_entry_t *assignment = NULL;*/
    /*bg_node_t *swNode;*/
    /*hw_node_t *hwNode;*/

    /*bg_graph_find_node(map->swGraph, swId, &swNode);*/
    /*hwNode = hw_graph_get_node(map->hwGraph, hwId);*/

    /*for (i = 0; i < swNode->input_port_cnt; ++i)*/
        /*for (j = 0; j < swNode->input_ports[i]->num_edges; ++j)*/
        /*{*/
            /*if ((assignment = sw2hw_map_get_assignment(map, swNode->input_ports[i]->edges[j]->source_node->id))*/
                    /*&& (assignment->hwId != hwNode->id))*/
                /*cost++;*/
            /*norm++;*/
        /*}*/

    /*for (i = 0; i < swNode->output_port_cnt; ++i)*/
        /*for (j = 0; j < swNode->output_ports[i]->num_edges; ++j)*/
        /*{*/
            /*if ((assignment = sw2hw_map_get_assignment(map, swNode->output_ports[i]->edges[j]->sink_node->id))*/
                    /*&& (assignment->hwId != hwNode->id))*/
                /*cost++;*/
            /*norm++;*/
        /*}*/

    /*return cost * 100 / norm;*/
/*}*/

/*This counts only assignments NOT real size!*/
static int costByAssignments(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId)
{
    int norm = 0;
    int current = 0;
    sw2hw_map_entry_t *entry;
    priority_list_iterator_t it;
    bg_node_t *swNode;
    hw_node_t *hwNode;

    bg_graph_find_node(map->swGraph, swId, &swNode);
    hwNode = hw_graph_get_node(map->hwGraph, hwId);

    norm = bg_node_list_size(map->swGraph->input_nodes);
    norm += bg_node_list_size(map->swGraph->hidden_nodes);
    norm += bg_node_list_size(map->swGraph->output_nodes);
    norm /= priority_list_size(map->hwGraph->nodes);
    norm++;

    for (entry = priority_list_first(map->assignments, &it);
            entry;
            entry = priority_list_next(&it))
        if (entry->hwId == hwNode->id)
            current++;

    return current * 100 / norm;
}

sw2hw_error sw2hw_map_init(sw2hw_map_t *map, const char *hwGraphName, const char *swGraphName)
{
    strncpy(map->hwGraphName, hwGraphName, HWG_MAX_STRING_LENGTH);
    strncpy(map->swGraphName, swGraphName, bg_MAX_STRING_LENGTH);

    /*Load hw graph*/
    map->hwGraph = calloc(1, sizeof(hw_graph_t));
    hw_graph_init(map->hwGraph, 0, "", "");
    hw_graph_from_yaml_file(hwGraphName, map->hwGraph);

    /*Load sw graph*/
    bg_graph_alloc(&map->swGraph, "");
    bg_graph_from_yaml_file(swGraphName, map->swGraph);

    priority_list_init(&map->assignments);
    return SW2HW_ERR_NONE;
}

void sw2hw_map_destroy(sw2hw_map_t *map)
{
    /*Destroy hw graph*/
    hw_graph_destroy(map->hwGraph);
    free(map->hwGraph);

    /*Destroy sw graph*/
    bg_graph_free(map->swGraph);

    priority_list_deinit(map->assignments);
}

sw2hw_map_entry_t *sw2hw_map_get_assignment(sw2hw_map_t *map, const unsigned int swId)
{
    sw2hw_map_entry_t *entry;
    priority_list_iterator_t it;

    for (entry = priority_list_first(map->assignments, &it);
            entry;
            entry = priority_list_next(&it))
        if (entry->swId == swId)
            return entry;
    return NULL;
}

sw2hw_error sw2hw_map_assign(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId, const int prio, const bool fixed)
{
    sw2hw_map_entry_t *entry;
    priority_list_iterator_t it;

    /*Do not allow multiple assignments of a SW entity*/
    if (sw2hw_map_get_assignment(map, swId))
        return SW2HW_ERR_DUPLICATE_ASSIGNMENT;

    entry = calloc(1, sizeof(sw2hw_map_entry_t));
    if (!entry)
        return SW2HW_ERR_NOMEM;

    entry->swId = swId;
    entry->hwId = hwId;
    entry->fixed = fixed;

    priority_list_insert(map->assignments, entry, prio, &it);

    return SW2HW_ERR_NONE;
}

/*Match SOURCES & SINKS in HW to INPUTS & OUTPUTS in SW by matching names*/
sw2hw_error sw2hw_premap(sw2hw_map_t *map)
{
    sw2hw_error err = SW2HW_ERR_NONE;
    bg_node_t *node;
    bg_node_list_iterator_t it;
    hw_node_t *hwNode;
    priority_list_iterator_t it2;
    unsigned int i;

    fprintf(stderr, "SW2HW_PREMAP: Start mapping by name\n");
    /*Cycle through all INPUT and OUTPUT nodes of the sw graph and try to preassign them by name*/
    for (node = bg_node_list_first(map->swGraph->input_nodes, &it);
         node;
         node = bg_node_list_next(&it))
    {
        if (node->type->id != bg_NODE_TYPE_INPUT)
            continue;
        for (hwNode = priority_list_first(map->hwGraph->nodes, &it2);
             hwNode;
             hwNode = priority_list_next(&it2))
            for (i = 0; i < hwNode->numPorts; ++i)
                if (hwNode->ports[i].type == HW_PORT_TYPE_SOURCE)
                    if (strcmp(hwNode->ports[i].name, node->name) == 0)
                        if ((err = sw2hw_map_assign(map, hwNode->id, node->id,0,true)) != SW2HW_ERR_NONE)
                            return err;
    }

    for (node = bg_node_list_first(map->swGraph->output_nodes, &it);
         node;
         node = bg_node_list_next(&it))
    {
        if (node->type->id != bg_NODE_TYPE_OUTPUT)
            continue;
        for (hwNode = priority_list_first(map->hwGraph->nodes, &it2);
             hwNode;
             hwNode = priority_list_next(&it2))
            for (i = 0; i < hwNode->numPorts; ++i)
                if (hwNode->ports[i].type == HW_PORT_TYPE_SINK)
                    if (strcmp(hwNode->ports[i].name, node->name) == 0)
                        if ((err = sw2hw_map_assign(map, hwNode->id, node->id,0,true)) != SW2HW_ERR_NONE)
                            return err;
    }

    fprintf(stderr, "SW2HW_PREMAP: %u nodes assigned\n", (unsigned int)priority_list_size(map->assignments));

    return err;
}

/*All SW entities will be assigned to one and only one HW entity*/
sw2hw_error sw2hw_map(sw2hw_map_t *map)
{
    int cost;
    bg_node_t *node;
    bg_node_list_t *allNodes;
    bg_node_list_iterator_t it;
    hw_node_t *target;
    priority_list_iterator_t it2, it3;
    priority_list_t *possible;
    sw2hw_map_entry_t *entry;

    fprintf(stderr, "SW2HW_MAP: Start mapping\n");

    /*I. Produce a list of all sw entities*/
    priority_list_init(&possible);
    bg_node_list_init(&allNodes);
    for (node = bg_node_list_first(map->swGraph->input_nodes, &it);
            node;
            node = bg_node_list_next(&it))
        bg_node_list_append(allNodes, node);
    for (node = bg_node_list_first(map->swGraph->hidden_nodes, &it);
            node;
            node = bg_node_list_next(&it))
        bg_node_list_append(allNodes, node);
    for (node = bg_node_list_first(map->swGraph->output_nodes, &it);
            node;
            node = bg_node_list_next(&it))
        bg_node_list_append(allNodes, node);

    fprintf(stderr, "SW2HW_MAP: Found %u SW nodes\n", (unsigned int)bg_node_list_size(allNodes));

    while (bg_node_list_size(allNodes)>0)
    {
        /*For every unassigned node and target: Calculate costs of a possible assignment*/
        for (node = bg_node_list_first(allNodes, &it);
                node;
                node = bg_node_list_next(&it))
        {
            for (target = priority_list_first(map->hwGraph->nodes, &it2);
                    target;
                    target = priority_list_next(&it2))
            {
                entry = calloc(1, sizeof(sw2hw_map_entry_t));
                entry->hwId = target->id;
                entry->swId = node->id;
                cost = costByNeighbours(map, target->id, node->id) + costByAssignments(map, target->id, node->id);
                priority_list_insert(possible, entry, cost, &it3);
            }
        }
        /*At the head of possible assignment list is the one with lowest cost!*/
        entry = priority_list_first(possible, &it2);
        fprintf(stderr, "SW2HW_MAP: Assigning %u to %u\n", entry->swId, entry->hwId);
        if (sw2hw_map_assign(map, entry->hwId, entry->swId, priority_list_get_priority(&it3), false) == SW2HW_ERR_DUPLICATE_ASSIGNMENT)
            fprintf(stderr, "SW2HW_MAP: %u already assigned. Skipping\n", entry->swId);

        /*Remove assigned node from allNodes*/
        bg_graph_find_node(map->swGraph, entry->swId, &node);
        bg_node_list_find(allNodes, node, &it);
        bg_node_list_erase(&it);

        /*Destroy all other possible assignments*/
        for (entry = priority_list_first(possible, &it3);
                entry;
                entry = priority_list_next(&it3))
            free(entry);
        priority_list_clear(possible);
    }

    bg_node_list_deinit(allNodes);
    priority_list_deinit(possible);

    fprintf(stderr, "SW2HW_MAP: Mapping finished\n");
    return SW2HW_ERR_NONE;
}

struct sw2hw_map_permutation_t {
    priority_list_t *group1;
    priority_list_t *group2;
};
typedef struct sw2hw_map_permutation_t sw2hw_map_permutation_t;

sw2hw_error sw2hw_map_refine(sw2hw_map_t *map)
{
    int cost, prevCost;
    priority_list_t *groups;
    priority_list_t *group, *group2;
    priority_list_t *possible;
    priority_list_iterator_t it, it2, it3;
    sw2hw_map_entry_t *entry, *entry2;
    sw2hw_map_permutation_t *perm;
    unsigned int hwId, hwId2;
    bool skip = false;

    fprintf(stderr, "SW2HW_MAP_REFINE: Starting global optimization\n");
    priority_list_init(&groups);
    priority_list_init(&possible);

    /*Build priority list of priority lists of assignments with same target*/
    for (entry = priority_list_first(map->assignments, &it);
            entry;
            entry = priority_list_next(&it))
    {
        /*Skip fixed assignments: It will never get added again*/
        if (entry->fixed)
            continue;
        /*Check if there is a group with entry->hwId*/
        for (group = priority_list_first(groups, &it2);
                group;
                group = priority_list_next(&it2))
        {
            entry2 = priority_list_first(group, &it3);
            if (entry2 && entry2->hwId == entry->hwId)
                break;
        }
        /*Recalculate costs*/
        cost = costByNeighbours(map, entry->hwId, entry->swId) + costByAssignments(map, entry->hwId, entry->swId);
        if (group)
        {
            /*Found group, update costs*/
            priority_list_insert(group, entry, cost, &it2);
            priority_list_find(groups, group, &it2);
            priority_list_insert(groups, group, priority_list_get_priority(&it2)+cost, &it3);
        } else {
            /*No group, create one*/
            priority_list_init(&group);
            priority_list_insert(group, entry, cost, &it2);
            priority_list_insert(groups, group, cost, &it3);
        }
    }

    for (group = priority_list_last(groups, &it);
            group && !skip;
            group = priority_list_prev(&it))
    {
        entry = priority_list_first(group, &it2);
        hwId = entry->hwId;
        fprintf(stderr, "SW2HW_MAP_REFINE: Group %u costs %d\n", hwId, priority_list_get_priority(&it));
    }

    /*The idea: Try to find a permutation of groups which decreases global cost*/
    for (group = priority_list_last(groups, &it);
            group && !skip;
            group = priority_list_prev(&it))
    {
        entry = priority_list_first(group, &it2);
        hwId = entry->hwId;
        fprintf(stderr, "SW2HW_MAP_REFINE: Selected group %u\n", hwId);
        
        memcpy(&it2, &it, sizeof(priority_list_iterator_t));
        for (group2 = priority_list_get(&it2);
                group2 && !skip;
                group2 = priority_list_prev(&it2))
        {
            if (group2 == group)
                continue;
            entry2 = priority_list_first(group2, &it3);
            hwId2 = entry2->hwId;
            fprintf(stderr, "SW2HW_MAP_REFINE: Target group %u\n", hwId2);
            prevCost = priority_list_get_priority(&it) + priority_list_get_priority(&it2);
            fprintf(stderr, "Previous Cost: %d\n", prevCost);
            /*Now we virtually assign all entries in group to hwId of group2 and vice versa*/
            for (entry = priority_list_first(group2, &it3);
                    entry;
                    entry = priority_list_next(&it3))
                entry->hwId = hwId;
            for (entry = priority_list_first(group, &it3);
                    entry;
                    entry = priority_list_next(&it3))
                entry->hwId = hwId2;

            /*Then we recalculate costs*/
            cost = 0;
            for (entry = priority_list_first(group2, &it3);
                    entry;
                    entry = priority_list_next(&it3))
                cost += costByNeighbours(map, hwId, entry->swId) + costByAssignments(map, hwId, entry->swId);
            for (entry = priority_list_first(group, &it3);
                    entry;
                    entry = priority_list_next(&it3))
                cost += costByNeighbours(map, hwId2, entry->swId) + costByAssignments(map, hwId2, entry->swId);
            fprintf(stderr, "New Cost: %d\n", cost);
            /*Undo virtual assignment*/
            for (entry = priority_list_first(group2, &it3);
                    entry;
                    entry = priority_list_next(&it3))
                entry->hwId = hwId2;
            for (entry = priority_list_first(group, &it3);
                    entry;
                    entry = priority_list_next(&it3))
                entry->hwId = hwId;

            /*If its less we make the changes permanent*/
            if (prevCost > cost)
            {
                fprintf(stderr, "SW2HW_MAP_REFINE: Registering possible permutation\n");
                perm = calloc(1, sizeof(sw2hw_map_permutation_t));
                perm->group1 = group;
                perm->group2 = group2;
                priority_list_insert(possible, perm, cost - prevCost, &it3);
            }
        }
    }

    perm = priority_list_first(possible, &it);
    if (perm) {
        entry = priority_list_first(perm->group1, &it);
        entry2 = priority_list_first(perm->group2, &it);
        hwId = entry->hwId;
        hwId2 = entry2->hwId;
        fprintf(stderr, "SW2HW_MAP_REFINE: Performing best permutation of group %u and %u\n", hwId, hwId2);
        /*Make virtual assignment real*/
        for (entry = priority_list_first(perm->group2, &it3);
                entry;
                entry = priority_list_next(&it3))
            entry->hwId = hwId;
        for (entry = priority_list_first(perm->group1, &it3);
                entry;
                entry = priority_list_next(&it3))
            entry->hwId = hwId2;
    }

    /*Update & Cleanup*/
    for (group = priority_list_first(groups, &it);
            group;
            group = priority_list_next(&it))
    {
        for (entry = priority_list_first(group, &it2);
                entry;
                entry = priority_list_next(&it2))
        {
            cost = costByNeighbours(map, entry->hwId, entry->swId) + costByAssignments(map, entry->hwId, entry->swId);
            priority_list_insert(map->assignments, entry, cost, &it3);
        }
        priority_list_deinit(group);
    }

    for (perm = priority_list_first(possible, &it);
            perm;
            perm = priority_list_next(&it))
        free(perm);
    priority_list_deinit(possible);
    priority_list_deinit(groups);
    fprintf(stderr, "SW2HW_MAP_REFINE: Optimization finished\n");

    return SW2HW_ERR_NONE;
}