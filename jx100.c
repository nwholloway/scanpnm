/*
 *	Nick Holloway <alfie@dcs.warwick.ac.uk>, 13th January 1994
 */
# include <stdio.h>
# include <string.h>
# include <termios.h>
# include <errno.h>
# include <sys/file.h>
# include <sys/time.h>
# include <fcntl.h>
# ifdef linux
#  include <linux/fs.h>
#  include <linux/tty.h>
# endif

# include "jx100.h"

# ifdef linux
struct serial_struct serial;
# endif

# define TIMEOUT 50		/* default timeout for next read (msecs) */

/* needs to be large enough to store longest scanline (100 * 0.04" * 400dpi) */
static u_char scratch [ 1600 ];


/* information shared between various jx100_* routines */
static int	n,			/* width of scan in pixels */
		l,			/* height of scan in pixels */
		linebytes,		/* length of scanline in bytes */
		scanlines,		/* number of scanlines left to read */
		handshake,		/* handshake scanner during scan? */
		plane,			/* what colour plane is being done */
		fudgepbm,		/* invert mono scans to match pbm */
		timeout	= TIMEOUT;	/* timeout for next read (msecs) */
static void     (*status) ( char * );	/* callback to provide verbose status */
static int	scanfd	= -1;		/* fd to communicate with scanner */
static struct	termios	tt,		/* terminal state to play with */
			tt_old;		/* original terminal state to restore */

static char *planemsg[] = {
    "scanning",
    "scanning green plane",
    "scanning red plane",
    "scanning blue plane"
};

/* forward declaration of communication routines */
static void msleep ( int msecs );
static int  get ( char *, int );
static int  get_ack ();
static int  send ( char * );
static int  send_acked ( char * );
static int  send_ack ();

/*
 * jx100_reset
 *     Send a CAN character to the scanner to get it to reset.  This can
 *   be useful for aborting a scan.  One caveat is that the scanner
 *   resets its baud rate to 9600.
 *     It turns out that the scanner won't listen all of the time, and
 *   will even send some characters after being requested to reset.  Sigh.
 */
int jx100_reset ()
{
    if ( status )
	(*status) ( "resetting scanner" );
    scanlines = 0;
    if ( send ( "\x18" ) < 0 )		/* request a reset... */
	return -1;
    msleep ( 1000 );
    if ( send ( "\x18" ) < 0 )		/* ...and then request again */
	return -1;
    if ( cfgetispeed ( &tt ) != B9600 ) {		/* reset to 9600 */
	cfsetispeed ( &tt, B9600 );
	cfsetospeed ( &tt, B9600 );
	if ( tcsetattr ( scanfd, TCSANOW, &tt ) < 0 )
	    return -1;
#ifdef linux
	/* reset meaning of 38400 */
	(void) ioctl ( scanfd, TIOCGSERIAL, &serial );
	serial.flags &= ~ASYNC_SPD_MASK;
	(void) ioctl ( scanfd, TIOCSSERIAL, &serial );
#endif
    }
    /* we wait a bit, then discard any spurious characters that came in */
    msleep ( 1000 );
    if ( tcflush ( scanfd, TCIFLUSH ) < 0 )
	return -1;
    /* physical head movement during reset can take 3 - 10 seconds */
    timeout = 10000;
    if ( get_ack() == 0 )		/* got ack, good! */
	return 0;
    /* We didn't get an ack.  Wait a bit, discard chars, try again */
    msleep ( 1000 );
    if ( tcflush ( scanfd, TCIFLUSH ) < 0 )
	return -1;
    timeout = 10000;
    /* If we don't get it this time, give up */
    return get_ack ();
}

int jx100_open ( char *device )
{
    scanfd = open ( device, O_RDWR | O_NDELAY | O_EXCL );
    if ( scanfd < 0 )
	return -1;
    if ( tcgetattr ( scanfd, &tt ) < 0 )
	return -1;
    tt_old = tt;
    tt.c_cc[VMIN] = 1;
    tt.c_cc[VTIME] = 1;
    tt.c_lflag &= ~ ( ISIG | ICANON | PENDIN | IEXTEN
	    | ECHO | ECHOE | ECHOK | ECHONL ) ;
    tt.c_lflag |= NOFLSH;
    tt.c_iflag &= ~ ( BRKINT | PARMRK | INPCK | ISTRIP | INLCR
	    | IGNCR | ICRNL | IUCLC | IXON | IXOFF | IXANY | IMAXBEL );
    tt.c_iflag |= IGNBRK | IGNPAR;
    tt.c_oflag &= ~ ( OPOST );
    tt.c_cflag &= ~ ( CSTOPB | PARENB | CSIZE );
    tt.c_cflag |= CS8 | CLOCAL | CREAD;
    cfsetispeed ( &tt, B9600 );
    cfsetospeed ( &tt, B9600 );
    if ( tcsetattr ( scanfd, TCSANOW, &tt ) < 0 )
	return -1;
    if ( tcflush ( scanfd, TCIOFLUSH ) < 0 )
	return -1;
    return 0;
}

/*
 * jx100_query
 *   check that there is a scanner actually attached by attempting to
 *   query it.
 */
int jx100_query ()
{
    if ( send_acked ( "M" ) < 0 )
	return -1;
    if ( get ( scratch, 16 ) < 0 )
	return -1;
    if ( strncmp ( scratch, "S jx-100 V", 10 ) != 0 
	    || strncmp ( scratch + 14, "\r\n", 2 ) != 0 )
	return -1;
    if ( status ) {
	scratch [ 14 ] = '\0';
	(*status) ( scratch );
    }
    return 0;
}


void jx100_close ( )
{
    if ( scanfd < 0 )
	return;
    if ( scanlines > 0 ) {
	(void) jx100_reset ();
	scanlines = 0;
    }
    if ( jx100_hispeed ( 0 ) < 0 )
	(void) jx100_reset ();
    (void) tcsetattr ( scanfd, TCSANOW, &tt_old );
    (void) close ( scanfd );
    scanfd = -1;
}

int jx100_setthreshold ( int red, int grn, int blu, int mono )
{
    if ( scanlines )
	return -1;
    if ( red < 0 || red > 255 || grn < 0 || grn > 255 || blu < 0 || blu > 255
	    || mono < 0 || mono > 255 )
	return -1;
    sprintf ( scratch, "B0;%d/%d/%d/%d;", red, grn, blu, mono );
    return send_acked ( scratch );
}

int jx100_setinverse ( int flag )
{
    if ( scanlines )
	return -1;
    return send_acked ( flag ? "B2" : "B1" );
}

int jx100_setdpi ( int xdpi, int ydpi )
{
    if ( scanlines )
	return -1;
    if ( xdpi < 50 || xdpi > 400 || ydpi < 50 || ydpi > 400 )
	return -1;
    if ( xdpi == ydpi ) {
	switch ( xdpi ) {
	    case 200:
		return send_acked ( "D1" );
	    case 100:
		return send_acked ( "D3" );
	    case 50:
		return send_acked ( "D5" );
	}
    }
    sprintf ( scratch, "D0" "%d.00,%d.00", xdpi, ydpi );
    return send_acked ( scratch );
}

int jx100_setscanarea ( int x, int y, int w, int h )
{
    if ( scanlines )
	return -1;
    if ( x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > 100 || y + h > 160 )
	return -1;
    sprintf ( scratch, "A0%d,%d,%d,%d;", x, w, y, h );
    return send_acked ( scratch );
}

/*
 * jx100_hispeed
 *   switch into a higher baud rate.  Unfortunately, the scanner only
 *   supports 9600, 19200, 57600, 115200, and 172800 baud.  On most Unix
 *   systems this means that the best we can use is 19200.  Linux provides
 *   means to access the UART at a lower level and enable the wierder
 *   rates.
 */
int jx100_hispeed ( int flag )
{
    if ( scanlines )
	return -1;
    if ( flag && cfgetispeed ( &tt ) == B9600 ) {
#ifdef linux
	/* set meaning of 38400 to be 115200 */
	if ( ioctl ( scanfd, TIOCGSERIAL, &serial ) < 0 )
	    return -1;
	serial.flags &= ~ASYNC_SPD_MASK;
	serial.flags |= ASYNC_SPD_VHI;
	if ( ioctl ( scanfd, TIOCSSERIAL, &serial ) < 0 )
	    return -1;
	if ( send_acked ( "I1" "115200,N,8,1" ) < 0 )
	    return -1;
	cfsetispeed ( &tt, B38400 );
	cfsetospeed ( &tt, B38400 );
#else
	if ( send_acked ( "I1" "19200,N,8,1" ) < 0 )
	    return -1;
	cfsetispeed ( &tt, B19200 );
	cfsetospeed ( &tt, B19200 );
#endif
	if ( tcsetattr ( scanfd, TCSANOW, &tt ) < 0 )
	    return -1;
    } else if ( !flag && cfgetispeed ( &tt ) != B9600 ) {
	if ( send_acked ( "I1" "9600,N,8,1" ) < 0 )
	    return -1;
	cfsetispeed ( &tt, B9600 );
	cfsetospeed ( &tt, B9600 );
	if ( tcsetattr ( scanfd, TCSANOW, &tt ) < 0 )
	    return -1;
#ifdef linux
	/* reset meaning of 38400 -- since we have set, assume can reset */
	(void) ioctl ( scanfd, TIOCGSERIAL, &serial );
	serial.flags &= ~ASYNC_SPD_MASK;
	(void) ioctl ( scanfd, TIOCSSERIAL, &serial );
#endif
    }
    return 0;
}

char *jx100_getscanline ()
{
    u_char header[4], trailer[1];
    int error = 0;

    if ( scanlines == 0 )
	return NULL;

    if ( scanlines % l == 0 ) {		/* just starting new plane */
	timeout = 15000;
	if ( status )
	    (*status) ( planemsg [ plane++ ] );
    } else
	timeout = 150;			/* time to move to next line */

    if ( ! handshake ) {
	if ( get ( scratch, linebytes ) != linebytes )
	    return NULL;
    } else {
	for ( ; ; ) {
	    if ( error++ ) {
		/* 
		 * Ack!  The Sun seems to build up buffers of bad chars
		 * when communication goes astray.  So we slurp any extra
		 * characters floating around before requesting the retry.
		 * This seems to get things acceptable with only 1 retry
		 * needed.
		 */
		while ( get ( scratch, sizeof ( scratch ) ) > 0 )
		    ;
# if 0
		fprintf ( stderr, "%02x%02x%02x%02x %02x %d %d\n",
			header[0], header[1], header[2], header[3],
			trailer[0], scanlines, error );
# endif
		send ( "r" );
	    }
	    if ( get ( header, 4 ) != 4 
		    || get ( scratch, linebytes ) != linebytes
		    || get ( trailer, 1 ) != 1 )
		continue;
	    if ( header[0] != '\x02' 
		    || (int) header[1] + ( (int) header[2] << 8 ) != n
		    || header[3] != ( scanlines % l == 1 ? '\1' : '\0' ) )
		continue;
	    if ( trailer[0] != (u_char) '\xFE' )
		continue;
	    break;
	}
	send_ack ();
    }
    scanlines--;

    if ( scanlines == 0 ) {
	/* allow time for head to return to rest */
	timeout = 15000;
	/* gross hack -- scanner won't talk just after completing a scan */
	msleep ( 1000 );
    }

    if ( fudgepbm ) {
	int      i = linebytes;
	char   *cp = scratch;
	while ( i-- )
	    *cp++ ^= '\xFF';
    }

    return scratch;
}

int jx100_setlamp ( int flag )
{
    return send_acked ( flag ? "L1" : "L0" );
}

int jx100_startscan ( int *xpixels, int *ypixels, int *bpl, int *lines, 
	scantype fmt, int wanthandshake, int wanthwgamma )
{
    /* we can't disable gamma when not using handshaking operation */
    if ( ! wanthandshake && ! wanthwgamma )
	return -1;
    fudgepbm = 0;
    switch ( fmt ) {
	case ppm:
	    strcpy ( scratch, "C1" );
	    break;
	case ppmpri:
	    strcpy ( scratch, "C2" );
	    fudgepbm = 1;
	    break;
	case pgmred: case pgmgrn: case pgmblu:
	    if ( ! wanthandshake )
		return -1;
	    /* fallthru */
	case pgm: 
	    strcpy ( scratch, "C3" );
	    break;
	case pbmred: case pbmgrn: case pbmblu:
	    if ( ! wanthandshake )
		return -1;
	    /* fallthru */
	case pbm: 
	    strcpy ( scratch, "C4" );
	    fudgepbm = 1;
	    break;
	default:
	    return -1;
    }
    if ( ! wanthandshake ) {
	strcat ( scratch, "S" );
    } else {
	switch ( fmt ) {
	    case pbm: case pgm: 
		strcat ( scratch, wanthwgamma ? "s0" : "s4" );
		plane = 0;
		break;
	    case ppm: case ppmpri:
		strcat ( scratch, wanthwgamma ? "s0" : "s4" );
		plane = 1;
		break;
	    case pbmred: case pgmred:
		strcat ( scratch, wanthwgamma ? "s2" : "s6" );
		plane = 2;
		break;
	    case pbmgrn: case pgmgrn:
		strcat ( scratch, wanthwgamma ? "s1" : "s5" );
		plane = 1;
		break;
	    case pbmblu: case pgmblu:
		strcat ( scratch, wanthwgamma ? "s3" : "s7" );
		plane = 3;
		break;
	}
    }
    if ( send_acked ( scratch ) < 0 )
	return -1;
    if ( status )
	(*status) ( "waiting for scanner to warm up" );
    /* quoted warmup: 50 seconds at 20 degrees C + delta */
    timeout = 60000;
    if ( get ( scratch, 4 ) < 0 ) 
	return -1;
    n = (int) scratch[0] + ( (int) scratch[1] << 8 );
    l = (int) scratch[2] + ( (int) scratch[3] << 8 );
    if ( wanthandshake )
	send_ack ();
    switch ( fmt ) {
	case pbm: case pbmred: case pbmgrn: case pbmblu:
	    linebytes = ( n + 7 ) / 8;
	    scanlines = l;
	    break;
	case pgm: case pgmred: case pgmgrn: case pgmblu:
	    linebytes = n;
	    scanlines = l;
	    break;
	case ppm:
	    linebytes = n;
	    scanlines = l * 3;
	    break;
	case ppmpri:
	    linebytes = ( n + 7 ) / 8;
	    scanlines = l * 3;
	    break;
    }
    *xpixels = n;
    *ypixels = l;
    *bpl = linebytes;
    *lines = scanlines;
    handshake = wanthandshake;
    return 0;
}

void jx100_status ( void (*fn) ( char * ) )
{
    status = fn;
}

static void msleep ( int msecs )
{
    struct timeval tm;

    tm.tv_sec = msecs / 1000;
    tm.tv_usec = ( msecs % 1000 ) * 1000;
    (void) select ( 1, (fd_set*)0, (fd_set*)0, (fd_set*)0, &tm );
}

static int send_acked ( char *str )
{
    if ( scanfd < 0 )
	return -1;
    while ( *str ) {
	if ( write ( scanfd, str++, 1 ) != 1 || get_ack () < 0 )
	    return -1;
    }
    return 0;
}

static int send ( char * str )
{
    if ( scanfd < 0 )
	return -1;
    while ( *str ) {
	if ( write ( scanfd, str++, 1 ) != 1 )
	    return -1;
    }
    return 0;
}

static int send_ack ()
{
    return send ( "\x06" );
}

static int get_ack ()
{
    char c;
    if ( get ( &c, 1 ) != 1 || c != '\x06' )
	return -1;
    return 0;
}

static int get ( char *buffer, int len )
{
    struct timeval tm, tmx;
    fd_set fdset, fdsetx;
    int i, done = 0;
#if 0
    int z;
#endif

    if ( scanfd < 0 )
	return -1;
    /* set the timeout for 1st character using current timeout value */
    tmx.tv_sec = timeout / 1000;
    tmx.tv_usec = ( timeout % 1000 ) * 1000;
#if 0
    z = timeout;
#endif
    /* reset timeout to default value */
    timeout = TIMEOUT;
    tm.tv_sec = 0;			/* timeout for subsequent chars */
    tm.tv_usec = TIMEOUT * 1000;
    /* we only want to select on the scanner */
    FD_ZERO ( &fdset );
    FD_SET ( scanfd, &fdset );
    while ( len > done ) {
	fdsetx = fdset;
	i = select ( scanfd+1, &fdsetx, (fd_set*)0, (fd_set*)0, &tmx );
#if 0
	if ( z ) {
	    z -= tmx.tv_sec * 1000 + tmx.tv_usec / 1000;
	    if ( z ) {
		fprintf ( stderr, "%8d", z );
		z = 0;
	    }
	}
#endif
	tmx = tm;
	if ( i == 0 ) {
	    if ( errno == EINTR ) {	/* restart if interrupted */
		errno = 0;
		continue;
	    }
	    return done;		/* return what we've got so far */
	}
	i = read ( scanfd, buffer + done, len - done );
	done += i;
    }
    return len;
}
