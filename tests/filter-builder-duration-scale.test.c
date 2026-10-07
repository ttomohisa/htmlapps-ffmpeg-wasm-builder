#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../runners/filter-builder-duration-scale.h"

int main(void) {
    const struct { const char *expr; double scale; } valid[] = {
        {"PTS", 1}, {"PTS-STARTPTS", 1}, {"(PTS-STARTPTS)", 1},
        {"PTS/1.5", 2.0/3}, {"PTS / 0.5", 2}, {"PTS*2", 2},
        {"0.5*PTS", .5}, {"(PTS-STARTPTS)/1.5", 2.0/3},
        {"( PTS - STARTPTS ) * 2", 2}, {"0.5*(PTS-STARTPTS)", .5},
        {"PTS/(1.5)", 2.0/3}, {"1e-2*PTS", .01}
    };
    const char *invalid[] = {
        "", "N/(30*TB)", "PTS/0", "PTS/-2", "PTS/NaN", "PTS/inf",
        "PTS*0", "PTS/1.5junk", "PTS*2+3", "if(N,PTS,PTS*2)",
        "PTS/1e-999", "PTS*1e999", "PTS*(2", "PTS+STARTPTS"
    };
    for (unsigned i = 0; i < sizeof(valid)/sizeof(*valid); i++) {
        double scale = 0;
        assert(filter_builder_duration_scale(valid[i].expr, &scale) == 0);
        assert(fabs(scale - valid[i].scale) < 1e-12);
    }
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(*invalid); i++) {
        double scale = 0;
        assert(filter_builder_duration_scale(invalid[i], &scale) < 0);
    }
    puts("Filter Builder duration expression tests passed (26 cases).");
}
