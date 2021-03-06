#include <stdio.h>
#include <errno.h>
#include "ggraphstruct.h"

extern struct agraph graph[];		/* the graphs */
extern float   graphx, graphy;		/* a point on the graph */
extern int     curline;
extern int     curgraph;
extern int     maxlines;		/* number of lines read in */
extern int     errno;
extern char    firstline[]; /* first line of gremlin file */
extern char    sfirstline[]; /* first line of gremlin file */
extern int     xgridf;
extern int     ygridf;
extern int     xaxisf;
extern int     yaxisf;
extern int     xtickf;
extern int     ytickf;
extern int     xticklf;
extern int     yticklf;
extern int     titlef;
extern int     framef;
extern int     symbsw;
extern int     crossxsw;
extern int     crossysw;
extern int     legendf;
extern int     legendbox;
extern int     legendside;

extern FILE *outfile;		/* output file */
extern char *commands[];	/* commands */
extern char *justify_names[];	/* justification */
extern int debug; 		/* debugging switch */
extern int ycharsz[];		/* size of characters */
extern int xcharsz[];		/* origin array of characters */
extern int descenders[];	/* descender array of characters */
extern char graphname[];	/* name of graph file being drawn */
extern int version;		/* version to use */
