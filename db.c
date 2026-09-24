#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"

int lookup_product(const char *dbfile, int upc, double *price, char *name, size_t namelen) {
    FILE *f = fopen(dbfile, "r");
    if (!f) {
        return 0;
    }

    int code;
    char tmpname[64];
    double p;

    while (fscanf(f, "%d %63s %lf", &code, tmpname, &p) == 3) {
        if (code == upc) {
            strncpy(name, tmpname, namelen - 1);
            name[namelen - 1] = '\0';
            *price = p;
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}