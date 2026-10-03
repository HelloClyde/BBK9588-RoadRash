#include "road_resource_paths.h"
#include <assert.h>
#include <string.h>

static int b_available;

static int probe(const char *path)
{
    const char expected[] =
        "B:\\\xD3\xA6\xD3\xC3\\\xCA\xFD\xBE\xDD\\\xD3\xCE\xCF\xB7\\Rash\\rashOpt.rsrc";
    assert(strcmp(path, expected) == 0);
    return b_available;
}

int main(void)
{
    char path[ROAD_RESOURCE_PATH_CAP];
    char short_path[8];
    b_available = 1;
    assert(strcmp(road_resource_select_root(probe), ROAD_RESOURCE_ROOT_B) == 0);
    assert(road_resource_join(path, sizeof(path), ROAD_RESOURCE_ROOT_B,
                              "Highway\\Highwayopt.rsrc"));
    assert(path[0] == 'B' && strstr(path, "\\Rash\\Highway\\Highwayopt.rsrc"));
    b_available = 0;
    assert(strcmp(road_resource_select_root(probe), ROAD_RESOURCE_ROOT_A) == 0);
    assert(road_resource_join(path, sizeof(path), ROAD_RESOURCE_ROOT_A,
                              "Streams\\bgaudio\\RashIF.RSRC"));
    assert(path[0] == 'A' && strstr(path, "\\Rash\\Streams\\bgaudio\\RashIF.RSRC"));
    assert(!road_resource_join(short_path, sizeof(short_path),
                               ROAD_RESOURCE_ROOT_A, "rashOpt.rsrc"));
    return 0;
}
