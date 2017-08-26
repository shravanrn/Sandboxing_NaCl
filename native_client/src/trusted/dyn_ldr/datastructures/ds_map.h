#ifndef DYN_LDR_DS_MAP_H__
#define DYN_LDR_DS_MAP_H__ 1

#include <stdio.h>
#include "native_client/src/shared/platform/nacl_log.h"

#define MAP_KEY_TYPE uint32_t
#define MAP_VALUE_TYPE uintptr_t
#define MAP_ARR_SIZE 32

struct _DS_Map {
    MAP_KEY_TYPE keys[MAP_ARR_SIZE];
    MAP_VALUE_TYPE values[MAP_ARR_SIZE];
    unsigned currentTop;
};

typedef struct _DS_Map DS_Map;

inline void Map_Init(DS_Map* map)
{
	map->currentTop = 0;
}

inline void Map_Put(DS_Map* map, MAP_KEY_TYPE key, MAP_VALUE_TYPE value)
{
	if(map->currentTop == MAP_ARR_SIZE)
	{
		NaClLog(LOG_FATAL, "Thread map put overflow\n");
        return;
	}

	map->keys[map->currentTop] = key;
	map->values[map->currentTop] = value;

	map->currentTop++;
}

inline MAP_VALUE_TYPE Map_Get(DS_Map* map, MAP_KEY_TYPE key)
{
	for(unsigned i = 0; i < MAP_ARR_SIZE; i++)
	{
		if(map->keys[i] == key)
		{
			return map->values[i];
		}
	}

	return 0;
}

#endif