#include <stdio.h>
#include <string.h>
#include "protocol.h"

/* Opens the product database file, searches for a UPC code, 
 * and extracts the price and item name if found. 
 * Returns 1 if found, 0 if not found or on file error. */
int lookup_product(const char *dbfile, int upc, double *price, char *name, size_t namelen) {
    FILE *f = fopen(dbfile, "r");
    if (!f) {
        return 0; // File couldn't be opened
    }

    int code;
    char tmpname[64];
    double p;

    /* Read line by line: format expects code, name (no spaces), and decimal price */
    while (fscanf(f, "%d %63s %lf", &code, tmpname, &p) == 3) {
        if (code == upc) {
            *price = p;
            strncpy(name, tmpname, namelen);
            name[namelen - 1] = '\0'; // Ensure safe null-termination
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0; // Reached end of file without finding the code
}