/*
 *	Nick Holloway <alfie@dcs.warwick.ac.uk>, 11th January 1994
 */

/*
 * The default tty device to open for the scanner
 */
# ifndef DEVICE
#  define DEVICE "/dev/scanner"
# endif

/* 
 * Template for temporary files.  The directory is TMPDIR (overridden by
 * $TMPDIR) and the template is TMPNAM (which must have leading '/' and 6
 * trailing 'X's.
 */
# ifndef TMPDIR
#  define TMPDIR "/var/tmp"
# endif
# ifndef TMPNAM
#  define TMPNAM "/scanpnmXXXXXX"
# endif

/*
 * the default resolution to do scanning at
 */
# ifndef DEFDPI
#  define DEFDPI 200
# endif

/*
 * the default type of image to produce
 */
# ifndef DEFFMT
#  define DEFFMT "ppm"
# endif

/*
 * These values are properties of the scanner
 */
# define MAXWIDTH   100
# define MAXHEIGHT  160
