/*
 *	Nick Holloway <alfie@dcs.warwick.ac.uk>, 11th January 1994
 */
typedef enum { 
    pbm, pbmred, pbmblu, pbmgrn,
    pgm, pgmred, pgmblu, pgmgrn,
    ppm, ppmpri
} scantype;

# ifdef __cplusplus
extern "C" {
# endif

extern int   jx100_reset ();
extern int   jx100_open ( char *device );
extern int   jx100_query ();
extern int   jx100_setdpi ( int xdpi, int ydpi );
extern int   jx100_setlamp ( int flag );
extern int   jx100_setthreshold ( int red, int grn, int blu, int mono );
extern int   jx100_setinverse ( int flag );
extern int   jx100_setscanarea ( int x, int y, int w, int h );
extern int   jx100_startscan ( int *xpixels, int *ypixels, int *bpl, int *lines,
			    scantype fmt, int wanthandshake, int wanthwgamma );
extern char *jx100_getscanline ();
extern int   jx100_hispeed ( int flag );
extern void  jx100_close ();
extern void  jx100_status ( void (*fn)(char *) );

#ifdef __cplusplus
}
#endif
