#include "sw_to_hw_map.h"
#include "sw_to_hw_map_yaml.h"
/*#include "bg_graph_yaml_loader.h"*/
/*#include "hwg_yaml_loader.h"*/

#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static sw2hw_parse_error get_event(yaml_parser_t *parser, yaml_event_t *event,
                          yaml_event_type_t type);
static sw2hw_parse_error get_event(yaml_parser_t *parser, yaml_event_t *event,
                          yaml_event_type_t type) {
  if(!yaml_parser_parse(parser, event)) {
    fprintf(stderr, "SW2HW_GET_EVENT: YAML parser encountered an error.\n");
    return SW2HW_PARSE_ERR_UNKNOWN;
  }
  if(event->type != type) {
    fprintf(stderr, "SW2HW_GET_EVENT: Expected %d got %d.\n",
            type, event->type);

    fprintf(stderr, "YAML_SCALAR_EVENT: %d\n", YAML_SCALAR_EVENT);
    fprintf(stderr, "YAML_SEQUENCE_START_EVENT: %d\n", YAML_SEQUENCE_START_EVENT);
    fprintf(stderr, "YAML_SEQUENCE_END_EVENT: %d\n", YAML_SEQUENCE_END_EVENT);
    fprintf(stderr, "YAML_MAPPING_START_EVENT: %d\n", YAML_MAPPING_START_EVENT);
    fprintf(stderr, "YAML_MAPPING_END_EVENT: %d\n", YAML_MAPPING_END_EVENT);

    return SW2HW_PARSE_ERR_UNKNOWN;
  }
  return SW2HW_PARSE_ERR_NONE;
}
static sw2hw_parse_error get_int(yaml_parser_t *parser, unsigned int *result) {
  sw2hw_parse_error err = SW2HW_PARSE_ERR_NONE;
  yaml_event_t event;

  if ((err = get_event(parser, &event, YAML_SCALAR_EVENT)) != SW2HW_PARSE_ERR_NONE)
    return err;

  *result = (unsigned int) atol((const char*)event.data.scalar.value);
  yaml_event_delete(&event);

  return err;
}
static sw2hw_parse_error get_string(yaml_parser_t *parser, char *s) {
  sw2hw_parse_error err = SW2HW_PARSE_ERR_NONE;
  yaml_event_t event;

  if ((err = get_event(parser, &event, YAML_SCALAR_EVENT)) != SW2HW_PARSE_ERR_NONE)
    return err;

  strncpy(s, (const char*)event.data.scalar.value, SW2HW_MAX_STRING_LENGTH);
  yaml_event_delete(&event);

  return err;
}

static sw2hw_parse_error parse_assignments (yaml_parser_t *parser, sw2hw_map_t *map)
{
  sw2hw_parse_error err = SW2HW_PARSE_ERR_NONE;
  yaml_event_t event;
    bool gotSwId = false;
    bool gotHwId = false;
    unsigned int swId, hwId;
    unsigned int costs = 0;
    unsigned int fixed = 0;

  err = get_event(parser, &event, YAML_SEQUENCE_START_EVENT);
  yaml_event_delete(&event);
  err = get_event(parser, &event, YAML_MAPPING_START_EVENT);
  do {
      gotSwId = false;
      gotHwId = false;
      costs = 0;
      fixed = 0;
      if (event.type == YAML_MAPPING_START_EVENT)
      {
          err = get_event(parser, &event, YAML_SCALAR_EVENT);
          do {
              /*Parse assignment contents*/
              if(strcmp((const char*)event.data.scalar.value, "swId") == 0)
              {
                  if (gotSwId)
                  {
                      fprintf(stderr, "SW2HW_PARSE_ASSIGNMENTS: Multiple \"swId\" sections.\n");
                  }
                  err = get_int(parser, &swId);
                  gotSwId = true;
              }
              else if(strcmp((const char*)event.data.scalar.value, "hwId") == 0)
              {
                  if (gotHwId)
                  {
                      fprintf(stderr, "SW2HW_PARSE_ASSIGNMENTS: Multiple \"hwId\" sections.\n");
                  }
                  err = get_int(parser, &hwId);
                  gotHwId = true;
              }
              else if(strcmp((const char*)event.data.scalar.value, "costs") == 0)
              {
                  err = get_int(parser, &costs);
              }
              else if(strcmp((const char*)event.data.scalar.value, "fixed") == 0)
              {
                  err = get_int(parser, &fixed);
              } else {
                  fprintf(stderr, "SW2HW_PARSE_ASSIGNMENTS: Ignoring unknown content\n");
              }

              /*Get next event*/
              yaml_event_delete(&event);
              if (!yaml_parser_parse(parser, &event))
                  err = SW2HW_PARSE_ERR_UNKNOWN;
          } while (err == SW2HW_PARSE_ERR_NONE && event.type != YAML_MAPPING_END_EVENT);

          /*Got new valid assignment: Set entries and push to list*/
          if (gotHwId && gotSwId)
          {
              sw2hw_map_assign(map, hwId, swId, costs, (fixed > 0));
          } else {
              fprintf(stderr, "SW2HW_PARSE_ASSIGNMENTS: Wrong assignment\n");
              err = SW2HW_PARSE_ERR_MISSING_ID;
          }
      }

      /*Get next event*/
      yaml_event_delete(&event);
      if (!yaml_parser_parse(parser, &event))
          err = SW2HW_PARSE_ERR_UNKNOWN;
  } while (err == SW2HW_PARSE_ERR_NONE && event.type != YAML_SEQUENCE_END_EVENT);

  if (err != SW2HW_PARSE_ERR_NONE)
  {
      fprintf(stderr, "SW2HW_PARSE_ASSIGNMENTS: Error parsing assignment %u to %u\n", swId, hwId);
      return err;
  }
  yaml_event_delete(&event);

  return err;
}

sw2hw_parse_error sw2hw_map_from_parser(yaml_parser_t *parser, sw2hw_map_t *map)
{
  sw2hw_parse_error err = SW2HW_PARSE_ERR_NONE;
  yaml_event_t event;
  bool gotHwName = false;
  bool gotSwName = false;
  char hwGraphName[HWG_MAX_STRING_LENGTH];
  char swGraphName[bg_MAX_STRING_LENGTH];

  if ((err = get_event(parser, &event, YAML_MAPPING_START_EVENT)) != SW2HW_PARSE_ERR_NONE) {
      fprintf(stderr, "SW2HW_MAP_FROM_PARSER: YAML parser encountered an error finding MAPPING_START.\n");
      return err;
  }
  yaml_event_delete(&event);
  if ((err = get_event(parser, &event, YAML_SCALAR_EVENT)) != SW2HW_PARSE_ERR_NONE) {
      fprintf(stderr, "SW2HW_MAP_FROM_PARSER: YAML parser encountered an error finding SCALAR.\n");
      return err;
  }
  do {
      /*Handle event*/
      if (event.type == YAML_SCALAR_EVENT)
      {
            if(strcmp("hwGraphName", (const char*)event.data.scalar.value) == 0) {
                if (gotHwName)
                {
                    fprintf(stderr, "SW2HW_MAP_FROM_PARSER: Multiple \"hwGraphName\" sections.\n");
                }
                err = get_string(parser, hwGraphName);
                gotHwName = true;
            } else if(strcmp("swGraphName",(const char*)event.data.scalar.value)==0) {
                if (gotSwName)
                {
                    fprintf(stderr, "SW2HW_MAP_FROM_PARSER: Multiple \"swGraphName\" sections.\n");
                }
                err = get_string(parser, swGraphName);
                gotSwName = true;
            }
            else if(strcmp((const char*)event.data.scalar.value, "assignments") == 0)
            {
                /*TODO: Postpone assignment parsing after initialization*/
                err = parse_assignments(parser, map);
            } else {
                fprintf(stderr, "SW2HW_MAP_FROM_PARSER: Scalar event %s unexpected\n", event.data.scalar.value);
            }
      }

      /*Initialize graph after getting both references*/
      if (gotHwName && gotSwName)
          sw2hw_map_init(map, hwGraphName, swGraphName);

      /*Get next event*/
      yaml_event_delete(&event);
      if (!yaml_parser_parse(parser, &event))
          err = SW2HW_PARSE_ERR_UNKNOWN;
  } while (err == SW2HW_PARSE_ERR_NONE && event.type != YAML_MAPPING_END_EVENT);

  return err;
}

sw2hw_parse_error sw2hw_map_from_yaml_file(const char *filename, sw2hw_map_t *map)
{
  sw2hw_parse_error err = SW2HW_PARSE_ERR_NONE;
  yaml_event_t event;
  yaml_parser_t parser;
  FILE *fp = NULL;

  yaml_parser_initialize(&parser);
  fp = fopen(filename, "r");
  if(!fp) {
    fprintf(stderr, "SW2HW_MAP_FROM_YAML_FILE: Could not open file \"%s\".\n", filename);
    return SW2HW_PARSE_ERR_IO;
  }
  yaml_parser_set_input_file(&parser, fp);

  /*Here we first handle events of the file itself: We can use hw_graph_from_parser() now also for nested hw graphs*/
  if(!yaml_parser_parse(&parser, &event)) {
    fprintf(stderr, "SW2HW_MAP_FROM_YAML_FILE: YAML parser encountered an error.\n");
    return SW2HW_PARSE_ERR_UNKNOWN;
  }
  if(event.type == YAML_STREAM_START_EVENT) {
    yaml_event_delete(&event);
    if(!yaml_parser_parse(&parser, &event)) {
        fprintf(stderr, "SW2HW_MAP_FROM_YAML_FILE: YAML parser encountered an error after STREAM_START.\n");
        return SW2HW_PARSE_ERR_UNKNOWN;
    }
    if(event.type == YAML_DOCUMENT_START_EVENT) {
      yaml_event_delete(&event);
      err = sw2hw_map_from_parser(&parser,map);
    }
  }

  yaml_parser_delete(&parser);
  fclose(fp);
  return err;
}
