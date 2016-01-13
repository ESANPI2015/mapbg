#include "sw_to_hw_map.h"
#include "sw_to_hw_map_yaml.h"
#include "hwg_yaml.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

bg_error bg_graph_to_emitter(yaml_emitter_t *emitter, const bg_graph_t *g);

static int emit_ulong(yaml_emitter_t *emitter, unsigned long x) {
  yaml_event_t event;
  int ok=1;
  char buf[SW2HW_MAX_STRING_LENGTH];
  sprintf(buf, "%lu", x);
  ok &= yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)buf,
                                     -1, 1, 0, YAML_PLAIN_SCALAR_STYLE);
  ok &= yaml_emitter_emit(emitter, &event);
  return ok;
}

static int emit_str(yaml_emitter_t *emitter, const char *s) {
  yaml_event_t event;
  int ok=1;
  ok &= yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)s,
                                     -1, 1, 0, YAML_PLAIN_SCALAR_STYLE);
  ok &= yaml_emitter_emit(emitter, &event);
  return ok;
}

static int emit_assignment_list(yaml_emitter_t *emitter, priority_list_t *assignment_list) {
  yaml_event_t event;
  sw2hw_map_entry_t *assignment;
  priority_list_iterator_t it;
  int ok = 1;

  for(assignment = priority_list_first(assignment_list, &it);
      assignment;
      assignment = priority_list_next(&it)) {

      ok &= yaml_mapping_start_event_initialize(&event, NULL, NULL, 0,
                                                YAML_FLOW_MAPPING_STYLE);
      ok &= yaml_emitter_emit(emitter, &event);

      ok &= emit_str(emitter, "hwId");
      ok &= emit_ulong(emitter, assignment->hwId);
      ok &= emit_str(emitter, "swId");
      ok &= emit_ulong(emitter, assignment->swId);
      ok &= emit_str(emitter, "costs");
      ok &= emit_ulong(emitter, priority_list_get_priority(&it));
      ok &= emit_str(emitter, "fixed");
      if (assignment->fixed)
          ok &= emit_ulong(emitter, 1);
      else
          ok &= emit_ulong(emitter, 0);
      ok &= yaml_mapping_end_event_initialize(&event);
      ok &= yaml_emitter_emit(emitter, &event);
  }
  return ok;
}

sw2hw_parse_error sw2hw_map_to_emitter(yaml_emitter_t *emitter, const sw2hw_map_t *map)
{
    sw2hw_parse_error err = SW2HW_PARSE_ERR_NONE;
  yaml_event_t event;
  int ok = 1;
  ok &= yaml_mapping_start_event_initialize(&event, NULL, NULL, 0,
                                            YAML_BLOCK_MAPPING_STYLE);
  ok &= yaml_emitter_emit(emitter, &event);
  /* emit graph properties*/
  ok &= emit_str(emitter, "hwGraph");
  hw_graph_to_emitter(emitter, map->hwGraph);
  ok &= emit_str(emitter, "swGraph");
  bg_graph_to_emitter(emitter, map->swGraph);
  /* emit edges */
  ok &= emit_str(emitter, "assignments");
  ok &= yaml_sequence_start_event_initialize(&event, NULL, NULL, 0,
                                             YAML_BLOCK_SEQUENCE_STYLE);
  ok &= yaml_emitter_emit(emitter, &event);
  ok &= emit_assignment_list(emitter, map->assignments);
  ok &= yaml_sequence_end_event_initialize(&event);
  ok &= yaml_emitter_emit(emitter, &event);
  /* finalize */
  ok &= yaml_mapping_end_event_initialize(&event);
  ok &= yaml_emitter_emit(emitter, &event);
  return err;
}

sw2hw_parse_error sw2hw_map_to_yaml_file(const char *filename, const sw2hw_map_t *map)
{
  yaml_emitter_t emitter;
  yaml_event_t event;
  int ok = 1;
  FILE *fp;
  sw2hw_parse_error err;

  fp = fopen(filename, "w");
  if(!fp) {
    return SW2HW_PARSE_ERR_IO;
  }
  ok &= yaml_emitter_initialize(&emitter);
  yaml_emitter_set_output_file(&emitter, fp);

  ok &= yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
  ok &= yaml_emitter_emit(&emitter, &event);
  ok &= yaml_document_start_event_initialize(&event, NULL, NULL, 0, 1);
  ok &= yaml_emitter_emit(&emitter, &event);

  err = sw2hw_map_to_emitter(&emitter, map);

  ok &= yaml_document_end_event_initialize(&event, 1);
  ok &= yaml_emitter_emit(&emitter, &event);
  ok &= yaml_stream_end_event_initialize(&event);
  ok &= yaml_emitter_emit(&emitter, &event);
  if(!ok) {
    fprintf(stderr, "YAML Error: %s\n", emitter.problem);
    err = SW2HW_PARSE_ERR_UNKNOWN;
  }

  yaml_emitter_delete(&emitter);
  fclose(fp);
  return err;
}
