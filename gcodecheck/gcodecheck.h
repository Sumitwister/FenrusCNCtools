#pragma once

extern void read_gcode(const char *filename);
extern void print_state(FILE *output);

extern void verify_fingerprint(const char *filename);

extern int verbose;
extern int errorcode;

#define vprintf(...) do { if (verbose) printf(__VA_ARGS__); } while (0)

#define error(...) do { fprintf(stderr, __VA_ARGS__); errorcode++;} while (0)

static inline double inch_to_mm(double inch) { return 25.4 * inch; };
static inline double mm_to_inch(double inch) { return inch / 25.4; };

static inline double ipm_to_metric(double inch) { return 25.4 * inch; };


extern "C" {
extern void set_tool_imperial(const char *name, int nr, double diameter_inch, double stepover_inch, double maxdepth_inch, double feedrate_ipm, double plungerate_ipm);
extern void read_tool_lib(const char *filename);
extern void activate_tool(int nr);
extern double get_tool_angle(int toolnr);
static inline double depth_to_radius(double d, double angle) { return fabs(d) * tan(angle/360.0 * M_PI); }

}