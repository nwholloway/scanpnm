/*
 *	Nick Holloway <alfie@dcs.warwick.ac.uk>, 11th February 1994
 */
# include <stdio.h>

/*
 * return 3 file pointers to the positions of the colour planes in the
 * file name passed to it
 */
static int initrgbfp ( FILE **rfp, FILE **gfp, FILE **bfp, char *file, int planesize )
{
    *rfp = fopen ( file, "r" );
    *gfp = fopen ( file, "r" );
    *bfp = fopen ( file, "r" );
    if ( ! rfp || ! gfp || ! bfp )
	return -1;
    /* colour planes are in the order G-R-B (not rgb) */
    if ( fseek ( *rfp, 1 * planesize, 0 ) < 0 
	    || fseek ( *bfp, 2 * planesize, 0 ) < 0 )
	return -1;
    return 0;
}

/*
 * combine the 8 bit rgb planes into 8 bit rgb triplets
 */
int combine8rgb ( char *file, int x, int y, FILE *ofp )
{
    int offset;
    int r, g, b;
    FILE *rfp, *gfp, *bfp;

    offset = x * y;

    if ( initrgbfp ( &rfp, &gfp, &bfp, file, offset ) < 0 )
	return -1;

    while ( offset-- ) {
	r = fgetc ( rfp );
	g = fgetc ( gfp );
	b = fgetc ( bfp );
	if ( r == EOF || g == EOF || b == EOF )
	    return -1;
	putchar ( r );
	putchar ( g );
	putchar ( b );
	if ( ferror ( ofp ) )
	    return -1;
    }
    fflush ( ofp );
    return ferror ( ofp ) ? -1 : 0;
}

/*
 * combine the 1 bit rgb planes into 8 bit rgb triplets
 */
int combine1rgb ( char *file, int x, int y, FILE *ofp )
{
    int offset, i, m;
    int r, g, b;
    FILE *rfp, *gfp, *bfp;

    offset = ( x + 7 ) / 8 * y;

    if ( initrgbfp ( &rfp, &gfp, &bfp, file, offset ) < 0 )
	return -1;

    while ( y-- ) {
	for ( i = 0; i < x; i++ ) {
	    m = 7 - ( i & 7 );
	    if ( m == 7 ) {
		r = fgetc ( rfp );
		g = fgetc ( gfp );
		b = fgetc ( bfp );
		if ( r == EOF || g == EOF || b == EOF )
		    return -1;
	    }
	    m = 1 << m;
	    putchar ( m & r ? '\0' : '\xFF' );
	    putchar ( m & g ? '\0' : '\xFF' );
	    putchar ( m & b ? '\0' : '\xFF' );
	    if ( ferror ( ofp ) )
		return -1;
	}
    }
    return 0;
}
