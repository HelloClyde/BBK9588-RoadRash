#ifndef ROAD_RESOURCE_PATHS_H
#define ROAD_RESOURCE_PATHS_H

/* Firmware filesystem paths are GBK byte strings, not UTF-8. */
#define ROAD_RESOURCE_DIRECTORY "\\\xD3\xA6\xD3\xC3\\\xCA\xFD\xBE\xDD\\\xD3\xCE\xCF\xB7\\Rash"
#define ROAD_RESOURCE_ROOT_B "B:" ROAD_RESOURCE_DIRECTORY
#define ROAD_RESOURCE_ROOT_A "A:" ROAD_RESOURCE_DIRECTORY
#define ROAD_RESOURCE_PATH_CAP 128u

static int road_resource_join(char *out, unsigned int capacity,
                              const char *root, const char *relative)
{
    unsigned int used = 0u;
    const char *part;
    if (!out || !root || !relative || !capacity) return 0;
    for (part = root; *part; ++part) {
        if (used + 1u >= capacity) return 0;
        out[used++] = *part;
    }
    if (*relative) {
        if (used + 1u >= capacity) return 0;
        out[used++] = '\\';
        for (part = relative; *part; ++part) {
            if (used + 1u >= capacity) return 0;
            out[used++] = *part;
        }
    }
    out[used] = '\0';
    return 1;
}

typedef int (*road_resource_exists_fn)(const char *path);

static const char *road_resource_select_root(road_resource_exists_fn exists)
{
    char probe[ROAD_RESOURCE_PATH_CAP];
    if (exists && road_resource_join(probe, sizeof(probe),
                                     ROAD_RESOURCE_ROOT_B, "rashOpt.rsrc") &&
        exists(probe)) return ROAD_RESOURCE_ROOT_B;
    return ROAD_RESOURCE_ROOT_A;
}

#endif
