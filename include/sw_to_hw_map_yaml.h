#ifndef _SW_TO_HW_MAP_YAML_H
#define _SW_TO_HW_MAP_YAML_H

#include <yaml.h>

typedef enum { 
    SW2HW_PARSE_ERR_NONE = 0,
    SW2HW_PARSE_ERR_IO,
    SW2HW_PARSE_ERR_MISSING_ID,
    SW2HW_PARSE_ERR_UNKNOWN
} sw2hw_parse_error;

sw2hw_parse_error sw2hw_map_from_parser(yaml_parser_t *parser, sw2hw_map_t *map);
sw2hw_parse_error sw2hw_map_from_yaml_file(const char *filename, sw2hw_map_t *map);
sw2hw_parse_error sw2hw_map_to_emitter(yaml_emitter_t *emitter, const sw2hw_map_t *map);
sw2hw_parse_error sw2hw_map_to_yaml_file(const char *filename, const sw2hw_map_t *map);

#endif
