#include "sw_to_hw_map.h"
#include "hwg_yaml.h"
#include "node_list.h"
#include "bg_graph.h"
#include "edge_list.h"
#include "node_types/bg_node_subgraph.h"
#include <stdio.h>
#include <string.h>

/*For external edges the cost are higher than for neighbouring edges which have higher costs than internal edges*/
int costByNeighbours(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId)
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
int costByExternalEdges(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId)
{
    int norm = 0;
    int cost = 0;
    unsigned int i,j;
    sw2hw_map_entry_t *assignment = NULL;
    bg_node_t *swNode;
    hw_node_t *hwNode;

    bg_graph_find_node(map->swGraph, swId, &swNode);
    hwNode = hw_graph_get_node(map->hwGraph, hwId);

    for (i = 0; i < swNode->input_port_cnt; ++i)
        for (j = 0; j < swNode->input_ports[i]->num_edges; ++j)
        {
            if ((assignment = sw2hw_map_get_assignment(map, swNode->input_ports[i]->edges[j]->source_node->id))
                    && (assignment->hwId != hwNode->id))
                cost++;
            norm++;
        }

    for (i = 0; i < swNode->output_port_cnt; ++i)
        for (j = 0; j < swNode->output_ports[i]->num_edges; ++j)
        {
            if ((assignment = sw2hw_map_get_assignment(map, swNode->output_ports[i]->edges[j]->sink_node->id))
                    && (assignment->hwId != hwNode->id))
                cost++;
            norm++;
        }

    return cost * 100 / norm;
}

/*This counts only assignments NOT real size!*/
int costByAssignments(sw2hw_map_t *map, const unsigned int hwId, const unsigned int swId)
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

sw2hw_error sw2hw_map_init(sw2hw_map_t *map, const hw_graph_t *hwGraph, const bg_graph_t *swGraph)
{
    /*Clone hw graph*/
    map->hwGraph = calloc(1, sizeof(hw_graph_t));
    hw_graph_init(map->hwGraph, 1, NULL);
    if (hwGraph)
        hw_graph_clone(map->hwGraph, hwGraph);

    /*Clone sw graph*/
    bg_graph_alloc(&map->swGraph, "");
    if (swGraph)
        bg_graph_clone(map->swGraph, swGraph);

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
sw2hw_error sw2hw_map_match(sw2hw_map_t *map)
{
    sw2hw_error err = SW2HW_ERR_NONE;
    bg_node_t *node;
    bg_node_list_iterator_t it;
    hw_node_t *hwNode;
    priority_list_iterator_t it2;
    unsigned int i;

    fprintf(stderr, "SW2HW_MAP_MATCH: Start mapping by name\n");
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

    fprintf(stderr, "SW2HW_MAP_MATCH: %u nodes assigned\n", (unsigned int)priority_list_size(map->assignments));

    return err;
}

int sw2hw_map_calculate_costs(sw2hw_map_t *map, costFuncPtr cost)
{
    int singleCost;
    int sum = 0;
    priority_list_t *copy;
    priority_list_iterator_t it, it2;
    sw2hw_map_entry_t *entry;

    /*Copy list of current assignments*/
    priority_list_init(&copy);
    priority_list_copy(copy, map->assignments);

    /*Cycle through copy and recalculate costs*/
    for (entry = priority_list_first(copy, &it);
            entry;
            entry = priority_list_next(&it))
    {
        singleCost = cost(map, entry->hwId, entry->swId);
        sum += singleCost;
        /*Update new costs in original list*/
        priority_list_insert(map->assignments, entry, singleCost, &it2);
    }

    priority_list_deinit(copy);

    return sum;
}

/*All SW entities will be assigned to one and only one HW entity*/
sw2hw_error sw2hw_map_initial(sw2hw_map_t *map, costFuncPtr cost)
{
    int singleCost;
    bg_node_t *node;
    bg_node_list_t *allNodes;
    bg_node_list_iterator_t it;
    hw_node_t *target;
    priority_list_iterator_t it2, it3;
    priority_list_t *possible;
    sw2hw_map_entry_t *entry;

    fprintf(stderr, "SW2HW_MAP_INITIAL: Start initial mapping\n");

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

    fprintf(stderr, "SW2HW_MAP_INITIAL: Found %u SW nodes\n", (unsigned int)bg_node_list_size(allNodes));

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
                singleCost = cost(map, target->id, node->id);
                priority_list_insert(possible, entry, singleCost, &it3);
            }
        }
        /*At the head of possible assignment list is the one with lowest cost!*/
        entry = priority_list_first(possible, &it2);
        fprintf(stderr, "SW2HW_MAP_INITIAL: Assigning %u to %u\n", entry->swId, entry->hwId);
        if (sw2hw_map_assign(map, entry->hwId, entry->swId, priority_list_get_priority(&it3), false) == SW2HW_ERR_DUPLICATE_ASSIGNMENT)
            fprintf(stderr, "SW2HW_MAP_INITIAL: %u already assigned. Skipping\n", entry->swId);

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

    fprintf(stderr, "SW2HW_MAP_INITIAL: Mapping finished\n");
    return SW2HW_ERR_NONE;
}

struct sw2hw_map_permutation_t {
    priority_list_t *group1;
    priority_list_t *group2;
};
typedef struct sw2hw_map_permutation_t sw2hw_map_permutation_t;

sw2hw_error sw2hw_map_refine(sw2hw_map_t *map, costFuncPtr cost)
{
    sw2hw_error err = SW2HW_ERR_NONE;
    int singleCost, global;
    priority_list_t *copy;
    priority_list_t *possible;
    priority_list_iterator_t it, it2, it3;
    sw2hw_map_entry_t *entry, *entry2;
    hw_node_t *target;
    unsigned int storedId;

    fprintf(stderr, "SW2HW_MAP_REFINE: Starting global optimization\n");
    priority_list_init(&possible);

    /* At first calculate initial global costs */
    global = sw2hw_map_calculate_costs(map, cost);
    fprintf(stderr, "SW2HW_MAP_REFINE: Initial global costs %d\n", global);

    /*Then we have to copy list, because assignments get modified by calculate costs*/
    priority_list_init(&copy);
    priority_list_copy(copy, map->assignments);

    /*For each assignment: try to find a better target*/
    for (entry = priority_list_last(copy, &it);
            entry;
            entry = priority_list_prev(&it))
    {
        /*Skip fixed entries*/
        if (entry->fixed)
            continue;
        /*Cycle through targets*/
        for (target = priority_list_first(map->hwGraph->nodes, &it2);
                target;
                target = priority_list_next(&it2))
        {
            /*Skip same target*/
            if (target->id == entry->hwId)
                continue;
            /*Virtually assign to new target*/
            storedId = entry->hwId;
            entry->hwId = target->id;
            /*Calculate new global costs*/
            singleCost = sw2hw_map_calculate_costs(map, cost);
            /*Undo assignment*/
            entry->hwId = storedId;
            /*Check assignment*/
            if (global > singleCost)
            {
                fprintf(stderr, "SW2HW_MAP_REFINE: Found possible target %u for %u\n", target->id, entry->swId);
                entry2 = calloc(1, sizeof(sw2hw_map_entry_t));
                entry2->hwId = target->id;
                entry2->swId = entry->swId;
                priority_list_insert(possible, entry2, singleCost - global, &it3);
            }
        }
    }

    entry = priority_list_first(possible, &it);
    if (entry)
    {
        entry2 = sw2hw_map_get_assignment(map, entry->swId);
        fprintf(stderr, "SW2HW_MAP_REFINE: Reassigning %u from %u to %u\n", entry->swId, entry2->hwId, entry->hwId);
        entry2->hwId = entry->hwId;
    } else {
        fprintf(stderr, "SW2HW_MAP_REFINE: No good reassignment found\n");
        err = SW2HW_ERR_NOT_FOUND;
    }

    global = sw2hw_map_calculate_costs(map, cost);
    fprintf(stderr, "SW2HW_MAP_REFINE: New global costs %d\n", global);

    /*Cleanup*/
    for (entry = priority_list_first(possible, &it);
            entry;
            entry = priority_list_next(&it))
        free(entry);
    priority_list_deinit(possible);
    priority_list_deinit(copy);
    fprintf(stderr, "SW2HW_MAP_REFINE: Optimization finished\n");

    return err;
}

sw2hw_error sw2hw_map_regroup(sw2hw_map_t *map, costFuncPtr cost)
{
    sw2hw_error err = SW2HW_ERR_NONE;
    int singleCost, global;
    priority_list_t *groups;
    priority_list_t *group, *group2;
    priority_list_t *possible;
    priority_list_iterator_t it, it2, it3;
    sw2hw_map_entry_t *entry, *entry2;
    sw2hw_map_permutation_t *perm;
    unsigned int hwId, hwId2;

    fprintf(stderr, "SW2HW_MAP_REGROUP: Starting global optimization\n");
    priority_list_init(&groups);
    priority_list_init(&possible);

    /* At first calculate initial global costs */
    global = sw2hw_map_calculate_costs(map, cost);
    fprintf(stderr, "SW2HW_MAP_REGROUP: Initial global costs %d\n", global);

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
        if (group)
        {
            /*Found group, update costs*/
            priority_list_insert(group, entry, priority_list_get_priority(&it), &it2);
            priority_list_find(groups, group, &it2);
            priority_list_insert(groups, group, priority_list_get_priority(&it2)+priority_list_get_priority(&it), &it3);
        } else {
            /*No group, create one*/
            priority_list_init(&group);
            priority_list_insert(group, entry, priority_list_get_priority(&it), &it2);
            priority_list_insert(groups, group, priority_list_get_priority(&it), &it3);
        }
    }

    /*The idea: Try to find a permutation of groups which decreases global cost*/
    for (group = priority_list_last(groups, &it);
            group;
            group = priority_list_prev(&it))
    {
        entry = priority_list_first(group, &it2);
        hwId = entry->hwId;
        
        memcpy(&it2, &it, sizeof(priority_list_iterator_t));
        for (group2 = priority_list_get(&it2);
                group2;
                group2 = priority_list_prev(&it2))
        {
            if (group2 == group)
                continue;
            entry2 = priority_list_first(group2, &it3);
            hwId2 = entry2->hwId;
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
            singleCost = sw2hw_map_calculate_costs(map, cost);
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
            if (global > singleCost)
            {
                fprintf(stderr, "SW2HW_MAP_REGROUP: Found permutation of %u and %u\n", hwId, hwId2);
                perm = calloc(1, sizeof(sw2hw_map_permutation_t));
                perm->group1 = group;
                perm->group2 = group2;
                priority_list_insert(possible, perm, singleCost - global, &it3);
            }
        }
    }

    perm = priority_list_first(possible, &it);
    if (perm) {
        entry = priority_list_first(perm->group1, &it);
        entry2 = priority_list_first(perm->group2, &it);
        hwId = entry->hwId;
        hwId2 = entry2->hwId;
        fprintf(stderr, "SW2HW_MAP_REGROUP: Applying permutation %u and %u\n", hwId, hwId2);
        /*Make virtual assignment real*/
        for (entry = priority_list_first(perm->group2, &it3);
                entry;
                entry = priority_list_next(&it3))
            entry->hwId = hwId;
        for (entry = priority_list_first(perm->group1, &it3);
                entry;
                entry = priority_list_next(&it3))
            entry->hwId = hwId2;
    } else {
        fprintf(stderr, "SW2HW_MAP_REGROUP: No good permutation found\n");
        err = SW2HW_ERR_NOT_FOUND;
    }

    /*Cleanup*/
    for (group = priority_list_first(groups, &it);
            group;
            group = priority_list_next(&it))
        priority_list_deinit(group);

    for (perm = priority_list_first(possible, &it);
            perm;
            perm = priority_list_next(&it))
        free(perm);
    priority_list_deinit(possible);
    priority_list_deinit(groups);

    global = sw2hw_map_calculate_costs(map, cost);
    fprintf(stderr, "SW2HW_MAP_REGROUP: New global costs %d\n", global);

    fprintf(stderr, "SW2HW_MAP_REGROUP: Optimization finished\n");

    return err;
}

sw2hw_error sw2hw_map_create_subgraph(sw2hw_map_t *map, const unsigned int hwId, bg_graph_t *sub)
{
    sw2hw_error err = SW2HW_ERR_NONE;
    priority_list_t *group;
    priority_list_iterator_t it, it2;
    sw2hw_map_entry_t *entry, *entry2;
    bg_node_t *node;
    bg_edge_t *edge;
    hw_node_t *target;
    unsigned int maxId = 0;
    unsigned int i,j;

    priority_list_init(&group);

    /*Generate group with hwId*/
    for (entry = priority_list_first(map->assignments, &it);
            entry;
            entry = priority_list_next(&it))
        if (entry->hwId == hwId)
            priority_list_insert(group, entry, priority_list_get_priority(&it), &it2);

    /*For each entry: Clone the nodes and update maxId*/
    for (entry = priority_list_first(group, &it);
            entry;
            entry = priority_list_next(&it))
    {
        bg_graph_find_node(map->swGraph, entry->swId, &node);
        bg_graph_clone_node(sub, node);
        if (node->id > maxId)
            maxId = node->id;
    }
    maxId++;

    /*When all nodes have been cloned, wire them together. If a edge enters/leaves the subgraph, create an INPUT or OUTPUT node*/
    for (entry = priority_list_first(group, &it);
            entry;
            entry = priority_list_next(&it))
    {
        bg_graph_find_node(map->swGraph, entry->swId, &node);
        if (!node)
            continue;

        for (i = 0; i < node->input_port_cnt; ++i)
        {
            for (j = 0; j < node->input_ports[i]->num_edges; ++j)
            {
                /*Check if edge already exists*/
                bg_graph_find_edge(sub, node->input_ports[i]->edges[j]->id, &edge);
                if (edge)
                    continue;

                /*Check if source node is in group*/
                entry2 = sw2hw_map_get_assignment(map, node->input_ports[i]->edges[j]->source_node->id);
                if (entry2->hwId == hwId)
                {
                    fprintf(stderr, "SW2HW_MAP_CREATE_SUBGRAPH: Creating internal edge from %u to %u\n",
                            (unsigned int)node->input_ports[i]->edges[j]->source_node->id,
                            (unsigned int)node->input_ports[i]->edges[j]->sink_node->id);
                    /*Check if edge does not exist yet!*/
                    bg_graph_create_edge(
                            sub,
                            node->input_ports[i]->edges[j]->source_node->id,
                            node->input_ports[i]->edges[j]->source_port_idx,
                            node->input_ports[i]->edges[j]->sink_node->id,
                            node->input_ports[i]->edges[j]->sink_port_idx,
                            node->input_ports[i]->edges[j]->weight,
                            node->input_ports[i]->edges[j]->id
                            );
                } else {
                    fprintf(stderr, "SW2HW_MAP_CREATE_SUBGRAPH: Creating input\n");
                    bg_graph_create_input(
                            sub,
                            "",
                            maxId
                            );
                    bg_graph_create_edge(
                            sub,
                            maxId,
                            0,
                            node->input_ports[i]->edges[j]->sink_node->id,
                            node->input_ports[i]->edges[j]->sink_port_idx,
                            node->input_ports[i]->edges[j]->weight,
                            node->input_ports[i]->edges[j]->id
                            );
                    maxId++;
                }
            }
        }
        for (i = 0; i < node->output_port_cnt; ++i)
        {
            for (j = 0; j < node->output_ports[i]->num_edges; ++j)
            {
                /*Check if edge already exists*/
                bg_graph_find_edge(sub, node->output_ports[i]->edges[j]->id, &edge);
                if (edge)
                    continue;
                /*Check if sink node is in group*/
                entry2 = sw2hw_map_get_assignment(map, node->output_ports[i]->edges[j]->sink_node->id);
                if (entry2->hwId == hwId)
                {
                    fprintf(stderr, "SW2HW_MAP_CREATE_SUBGRAPH: Creating internal edge from %u to %u\n",
                            (unsigned int)node->output_ports[i]->edges[j]->source_node->id,
                            (unsigned int)node->output_ports[i]->edges[j]->sink_node->id);
                    bg_graph_create_edge(
                            sub,
                            node->output_ports[i]->edges[j]->source_node->id,
                            node->output_ports[i]->edges[j]->source_port_idx,
                            node->output_ports[i]->edges[j]->sink_node->id,
                            node->output_ports[i]->edges[j]->sink_port_idx,
                            node->output_ports[i]->edges[j]->weight,
                            node->output_ports[i]->edges[j]->id
                            );
                } else {
                    fprintf(stderr, "SW2HW_MAP_CREATE_SUBGRAPH: Creating output\n");
                    bg_graph_create_output(
                            sub,
                            "",
                            maxId
                            );
                    bg_graph_create_edge(
                            sub,
                            node->output_ports[i]->edges[j]->source_node->id,
                            node->output_ports[i]->edges[j]->source_port_idx,
                            maxId,
                            0,
                            1.0f, /*Leaving edges should not have a changing weight, because entering edges of other subgraphs do the calculation*/
                            node->output_ports[i]->edges[j]->id
                            );
                    maxId++;
                }
            }
        }
    }

    /*Finalize the graph: It inherits id of target*/
    target = hw_graph_get_node(map->hwGraph, hwId);
    sub->id = target->id;
    sub->eval_order_is_dirty = true;

    priority_list_deinit(group);
    return err;
}

sw2hw_error sw2hw_map_create_graph(sw2hw_map_t *map, bg_graph_t *graph)
{
    sw2hw_error err = SW2HW_ERR_NONE;
    priority_list_iterator_t it;
    bg_edge_list_iterator_t it2;
    hw_node_t *target;
    bg_edge_t *edge;
    bg_node_t *node, *subnode;
    bg_graph_t *sub;
    sw2hw_map_entry_t *src, *sink;
    unsigned int edgeId = 1;
    unsigned int i,j,k;
    unsigned int srcIdx, sinkIdx;
    bool skip;

    /*For each target: Create a subgraph and a subgraph node*/
    for (target = priority_list_first(map->hwGraph->nodes, &it);
            target;
            target = priority_list_next(&it))
    {
        bg_graph_alloc(&sub, target->name);
        sw2hw_map_create_subgraph(map, target->id, sub);
        bg_graph_create_node(graph, target->name, target->id, bg_NODE_TYPE_SUBGRAPH);
        bg_node_set_subgraph(graph, target->id, sub);
    }

    /*For each sw edge: Check if it connects two different subgraphs and wire them accordingly*/
    for (edge = bg_edge_list_first(map->swGraph->edge_list, &it2);
            edge;
            edge = bg_edge_list_next(&it2))
    {
        src = sw2hw_map_get_assignment(map, edge->source_node->id);
        sink = sw2hw_map_get_assignment(map, edge->sink_node->id);
        /*Skip edges in same subgraphs*/
        if (src->hwId == sink->hwId)
            continue;

        /*Now the tricky part: find the INPUT/OUTPUT pair to be connected
         * subgraphIds = hwIds, but the port indices have to be found*/
        bg_graph_find_node(graph, src->hwId, &node);
        sub = ((subgraph_data_t *)node->_priv_data)->subgraph;
        bg_graph_find_node(sub, src->swId, &subnode);
        skip = false;
        /*Idea:
         * * Cycle through all outgoing edges of the original source node
         * * Find the edge with the same id
         * * If found, find the sink node (OUTPUT node) and its corresponding output at the SUBGRAPH node
         * * Store the index and leave*/
        for (i = 0; (i < subnode->output_port_cnt) && !skip; ++i)
        {
            for (k = 0; (k < subnode->output_ports[i]->num_edges) && !skip; ++k)
            {
                if (subnode->output_ports[i]->edges[k]->id != edge->id)
                    continue;
                for (j = 0; (j < node->output_port_cnt) && !skip; ++j)
                {
                    if (subnode->output_ports[i]->edges[k]->sink_node->output_ports[0] == node->output_ports[j])
                    {
                        srcIdx = j;
                        skip = true;
                    }
                }
            }
        }

        bg_graph_find_node(graph, sink->hwId, &node);
        sub = ((subgraph_data_t *)node->_priv_data)->subgraph;
        bg_graph_find_node(sub, sink->swId, &subnode);
        skip = false;
        /*Idea:
         * * Cycle through all incoming edges of the original sink node
         * * Find the edge with the same id
         * * If found, find the source node (INPUT node) and its corresponding input at the SUBGRAPH node
         * * Store the index and leave*/
        for (i = 0; (i < subnode->input_port_cnt) && !skip; ++i)
        {
            for (k = 0; (k < subnode->input_ports[i]->num_edges) && !skip; ++k)
            {
                if (subnode->input_ports[i]->edges[k]->id != edge->id)
                    continue;
                for (j = 0; (j < node->input_port_cnt) && !skip; ++j)
                {
                    if (subnode->input_ports[i]->edges[k]->source_node->input_ports[0] == node->input_ports[j])
                    {
                        sinkIdx = j;
                        skip = true;
                    }
                }
            }
        }

        fprintf(stderr, "SW2HW_MAP_CREATE_GRAPH: Connecting (%u, %u) to (%u, %u)\n", src->hwId, srcIdx, sink->hwId, sinkIdx);
        /*Finally we create the new edge*/
        bg_graph_create_edge(
                graph,
                src->hwId,
                srcIdx,
                sink->hwId,
                sinkIdx,
                1.0f,
                edgeId
                );
        edgeId++;
    }

    return err;
}

sw2hw_error sw2hw_map_transform(sw2hw_map_t *dest, sw2hw_map_t *src)
{
    sw2hw_error err = SW2HW_ERR_NONE;
    hw_node_t *target;
    sw2hw_map_entry_t *entry;
    priority_list_iterator_t it, it2;
    int sum;

    hw_graph_clone(dest->hwGraph, src->hwGraph);
    sw2hw_map_create_graph(src, dest->swGraph);

    for (target = priority_list_first(dest->hwGraph->nodes, &it);
            target;
            target = priority_list_next(&it))
    {
        sum = 0;
        for (entry = priority_list_first(src->assignments, &it2);
                entry;
                entry = priority_list_next(&it2))
            if (entry->hwId == target->id)
                sum += priority_list_get_priority(&it2);
        sw2hw_map_assign(dest, target->id, target->id, sum, true);
    }

    return err;
}
