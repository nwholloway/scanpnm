/*
 *	Nick Holloway <alfie@dcs.warwick.ac.uk>, 11th January 1994
 */
# include <stdio.h>
# include <termios.h>
# include <errno.h>
# ifdef linux
#  include <getopt.h>
# else
extern char *optarg;
extern int   optind, opterr;
# endif
# include <string.h>
# include <unistd.h>
# include <stdlib.h>
# include <signal.h>
# include <sys/param.h>

# include "scanpnm.h"
# include "jx100.h"
# include "util.h"

char pbmhead[] = "P4\n# %s\n%d %d\n";		/* header for pbm file */
char pgmhead[] = "P5\n# %s\n%d %d\n255\n";	/* header for pgm file */
char ppmhead[] = "P6\n# %s\n%d %d\n255\n";	/* header for ppm file */

struct fmt {
    char     *str;
    scantype  type;
    char     *head;
} fmttable [] = {
    { "pbm",    pbm,    pbmhead },
    { "pbmred", pbmred, pbmhead },
    { "pbmblu", pbmblu, pbmhead },
    { "pbmgrn", pbmgrn, pbmhead },
    { "pgm",    pgm,    pgmhead },
    { "pgmred", pgmred, pgmhead },
    { "pgmblu", pgmblu, pgmhead },
    { "pgmgrn", pgmgrn, pgmhead },
    { "ppm",    ppm,    ppmhead },
    { "ppmpri", ppmpri, ppmhead },
    { NULL,     -1,     NULL }
};

char   *progname;
char    tmprgb [ MAXPATHLEN ];

void usage ( )
{
    fprintf ( stderr, "usage: %s [ -t type ] [ -d dpi ] [ -i ] [ -n ]"
	    " [ -x offset ] [ -y offset ] [ -w width ] [ -h height ]"
	    " [ -D device ] [ -v ]\n", progname );

    exit ( 1 );
}

void fatal ( char *s )
{
    jx100_close ();
    (void) fprintf ( stderr, "%s: %s\n", progname, s );
    if ( tmprgb[0] != '\0' )
	unlink ( tmprgb );
    exit ( 1 );
}

void report ( char *s )
{
    fprintf ( stderr, "%s\n", s );
}

void tidyup ()
{
    report ( "caught signal..." );
    fatal ( "killed" );
}
    
main ( int argc, char *argv[] )
{
    FILE *ofp;
    char *cp;
    char comment [ 80 ];
    int i, x, y, lines, bpl;
    struct fmt *fmtp;
    struct sigaction sigact;
    /* defaults */
    char   *device  = DEVICE;
    char   *fmt     = DEFFMT;
    int     dpi     = DEFDPI;
    int     xoffset = -1,
            yoffset = -1,
            width   = -1,
            height  = -1,
            inverse = 0,
            nogamma = 0,
            verbose = 0;

    progname = argv[0];

    while ( ( i = getopt ( argc, argv, "t:d:x:y:w:h:D:vin" ) ) != EOF ) {
	switch ( i ) {
	case 'v':
	    verbose++;
	    break;
	case 'n':
	    nogamma++;
	    break;
	case 't':
	    fmt = optarg;
	    break;
	case 'D':
	    device = optarg;
	    break;
	case 'i':
	    inverse++;
	    break;
	case 'd':
	    dpi = atol ( optarg );
	    break;
	case 'x':
	    xoffset = atol ( optarg );
	    break;
	case 'y':
	    yoffset = atol ( optarg );
	    break;
	case 'w':
	    width = atol ( optarg );
	    break;
	case 'h':
	    height = atol ( optarg );
	    break;
	default:
	    usage ();
	    break;
	}
    }
    if ( optind < argc ) {
	usage ();
    }

    /* look up the image format */
    fmtp = fmttable;
    while ( fmtp->str != NULL && strcmp ( fmtp->str, fmt ) != 0 )
	fmtp++;
    if ( fmtp->str == NULL )
	fatal ( "unknown image format" );

    /* If the scan area was not set on the command line, set it.  */
    if ( xoffset == -1 )
	xoffset = 0;
    if ( yoffset == -1 )
	yoffset = 0;
    if ( width == -1 )
	width = MAXWIDTH - xoffset;
    if ( height == -1 )
	height = MAXHEIGHT - yoffset;

    /* Check some of the parameters */
    if ( xoffset < 0 || yoffset < 0 || width <= 0 || height <= 0
	    || xoffset + width > MAXWIDTH || yoffset + height > MAXHEIGHT )
	fatal ( "bad setting for scan area" );
    if ( dpi < 50 || dpi > 400 )
	fatal ( "bad value for dpi" );

    /* Set up signal handlers to tidy up */
    sigact.sa_handler = &tidyup;
    sigfillset ( &sigact.sa_mask );
    sigact.sa_flags = SA_INTERRUPT;
    (void) sigaction ( SIGHUP, &sigact, (struct sigaction*) 0 );
    (void) sigaction ( SIGINT, &sigact, (struct sigaction*) 0 );
    (void) sigaction ( SIGQUIT, &sigact, (struct sigaction*) 0 );
    (void) sigaction ( SIGTERM, &sigact, (struct sigaction*) 0 );
    (void) sigaction ( SIGPIPE, &sigact, (struct sigaction*) 0 );

    /* If we are generating colour scans, we need to combine rgb
     * planes, so we need a temporary file
     */
    if ( fmtp->type == ppm || fmtp->type == ppmpri ) {
	if ( ( cp = getenv ( "TMPDIR" ) ) != NULL )
	    strcpy ( tmprgb, cp );
	else
	    strcpy ( tmprgb, TMPDIR );
	strcat ( tmprgb, TMPNAM );
	if ( mktemp ( tmprgb ) == NULL )
	    fatal ( "mktemp failed" );
	ofp = fopen ( tmprgb, "w" );
	if ( ofp == NULL )
	    fatal ( "can't create temp file" );
    } else {
	ofp = stdout;
    }

    /* OK, let's get on with the scanning! */
    if ( jx100_open ( device ) < 0 ) {
	fatal ( "can't open scanner device" );
    }
    if ( verbose )
	jx100_status ( report );
    if ( jx100_query () < 0 )
	fatal ( "can't talk to scanner" );
    if ( jx100_setdpi ( dpi, dpi ) )
	fatal ( "unable to set dpi" );
    if ( jx100_setscanarea ( xoffset, yoffset, width, height ) )
	fatal ( "unable to set scan area" );
    if ( jx100_setinverse ( inverse ) )
	fatal ( "unable to set inverse" );
    if ( jx100_hispeed ( 1 ) )
	fatal ( "can't set hispeed mode" );
    if ( jx100_startscan ( &x, &y, &bpl, &lines, fmtp->type, 1, !nogamma ) < 0 )
	fatal ( "unable to initiate scan" );
    /* print the image header */
    sprintf ( comment, "scanpnm: %s image, %.2f\" x %.2f\" at %d dpi", 
	    fmtp->str, width * 0.04, height * 0.04, dpi );
    fprintf ( stdout, fmtp->head, comment, x, y );
    while ( lines-- ) {
	cp = jx100_getscanline ();
	if ( cp == NULL )
	    fatal ( "error fetching scanline" );
	fwrite ( cp, 1, bpl, ofp );
	if ( ferror ( ofp ) )
	    fatal ( "write error" );
    }
    (void) jx100_hispeed ( 0 );
    jx100_close ();
    if ( fmtp->type == ppm || fmtp->type == ppmpri ) {
	if ( fclose ( ofp ) == EOF )
	    fatal ( "write error" );
	if ( fmtp->type == ppm ) {
	    if ( combine8rgb ( tmprgb, x, y, stdout ) < 0 )
		fatal ( "error combining ppm planes" );
	} else {
	    if ( combine1rgb ( tmprgb, x, y, stdout ) < 0 )
		fatal ( "error combining pbm planes" );
	}
	(void) unlink ( tmprgb );
    }
    fflush ( stdout );
    if ( ferror ( stdout ) )
	fatal ( "write error" );

    return 0;
}
