#include <iostream>
using namespace std;
#include <cerrno>					// errno
#include <cstdlib>					// atoi, exit, abort
#include <cstring>					// memset, string routines
#include <sys/socket.h>
#include <sys/param.h>					// howmany
#include <sys/un.h>
#include <fcntl.h>

#ifndef SUN_LEN
#define SUN_LEN(su) (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

const char EOD = '\377';
const unsigned int BufSize = 256;
// buffer must be retained between calls due to blocking write (see parameter "state")
char wbuf[BufSize+1];					// +1 for '\0'
bool wblk;
unsigned int rcnt, wcnt, written;


bool input( char *inmsg, int size ) {
    static bool eod = false;

  if ( eod ) return true;
    cin.get( inmsg, size, '\0' );			// leave room for string terminator
    if ( inmsg[0] == '\0' ) {				// eof ?
	inmsg[0] = EOD;					// create EOD message for server
	inmsg[1] = '\0';
	eod = true;					// remember EOD is sent
    } // if
    return false;
} // input

// Datagram sockets are lossy (i.e., drop packets). To prevent clients from flooding the server with packets, resulting
// in dropped packets, a semaphore is used to synchronize the reader and writer tasks so at most N writes occur before a
// read. As well, if the buffer size is increase substantially, it may be necessary to decrease N to ensure the server
// buffer does not fill.

enum { MaxWriteBeforeRead = 10 };
unsigned int cnt = MaxWriteBeforeRead;

bool reader( int sock ) {
    static char buf[BufSize];

    for ( ;; cnt += 1 ) {
	int rlen = recvfrom( sock, buf, BufSize, 0, NULL, NULL ); // read data from socket
	if ( rlen == -1 ) {				// problem ?
	    if ( errno != EWOULDBLOCK ) {		// real problem ?
		perror( "Error: client read" );
		abort();
	    } // if
	    return false;				// indicate blocking
	} // if
	//cerr << "reader rlen:" << rlen << endl;
	if ( rlen == 0 ) {				// EOF should not occur
	    cerr << "Error: client EOF ecountered without EOD" << endl;
	    abort();
	} // if
	rcnt += rlen;					// total amount read
	if ( buf[rlen - 1] == EOD ) {			// EOD marks all read
	    cout.write( buf, rlen - 1 );		// write out data
	    //cerr << "reader seen EOD" << endl;
	    return true;				// indicate EOD
	} // if
	cout.write( buf, rlen );			// write data
    } // for
} // reader


enum Wstatus { Blocked, Eod, Cont };

Wstatus writer( int sock, struct sockaddr_un &server, socklen_t saddrlen ) {
    static bool eod = false;

  if ( eod ) return Eod;
    for ( ; 0 < cnt; cnt -= 1 ) {
	if ( ! wblk ) {					// not write blocked ?
	    input( wbuf, BufSize+1 );			// read data if no previous blocking
	    written = 0;				// starting new write
	} // if
	for ( ;; ) {					// ensure all data is written
	    char *buf = wbuf + written;			// location to continue from in buffer
	    unsigned int len = strlen(wbuf) - written;	// amount left to write
	    int wlen = sendto( sock, buf, len, 0, (struct sockaddr *)&server, saddrlen ); // write data to socket
	    if ( wlen == -1 ) {				// problem ?
		if ( errno != EWOULDBLOCK
#ifdef __FreeBSD__
		     && errno != ENOBUFS
#endif // __FreeBSD__
		    ) {					// real problem ?
		    perror( "Error: client write" );
		    abort();
		} // if
		wblk = true;
		return Blocked;				// indicate blocking
	    } // if
	    wblk = false;
	    //cerr << "writer wlen:" << wlen << endl;
	    wcnt += wlen;				// total amount written
	    if ( wbuf[wlen - 1] == EOD ) {		// EOD marks all written
		eod = true;
		//cerr << "writer EOD" << endl;
		return Eod;				// indicate EOD
	    } // if
	    written += wlen;				// current amount written
	  if ( written == strlen(wbuf) ) break;		// transferred across all writes
	} // for
    } // for
    return Cont;
} // writer


void manager( int sock, struct sockaddr_un &server, socklen_t saddrlen ) {
    static fd_set mrfds, rfds, mwfds, wfds;		// zero filled
    Wstatus wstatus = Cont;

    for ( ;; ) {
	//cerr << "manager1  wstatus:" << wstatus << " mwfds:" << mwfds.fds_bits[0] << " mrfds:" << mrfds.fds_bits[0] << endl;
	if ( ! FD_ISSET( sock, &mwfds ) ) {		// not waiting ?
	    wstatus = writer( sock, server, saddrlen );
	    if ( wstatus == Blocked ) {
		FD_SET( sock, &mwfds );			// blocking, set mask
	    } // if
	} // if
	if ( ! FD_ISSET( sock, &mrfds ) ) {		// not waiting ?
	    if ( reader( sock ) ) break;		// EOD ?
	    FD_SET( sock, &mrfds );			// otherwise blocking, set mask
	} // if
	// both blocking or writer finished or counter indicates no more writing and reader blocking ?
	if ( ( FD_ISSET( sock, &mwfds ) || wstatus == Eod || cnt == 0 ) && FD_ISSET( sock, &mrfds ) ) {
	    //cerr << "manager2  wstatus:" << wstatus << " mwfds:" << mwfds.fds_bits[0] << " mrfds:" << mrfds.fds_bits[0] << endl;
	    rfds = mrfds;				// copy for destructive usage
	    wfds = mwfds;
	    int nfs = pselect( sock+1, &rfds, &wfds, NULL, NULL, NULL ); // wait for one or the other
	    if ( nfs <= 0 ) {
		perror( "Error: pselect" );
		abort();
	    } // if

	    //cerr << "manager3  nfds:" << nfs << " wfds:" << wfds.fds_bits[0] << " rfds:" << rfds.fds_bits[0] << endl;
	    if ( FD_ISSET( sock, &mwfds ) && FD_ISSET( sock, &wfds ) ) { // waiting for data and data ?
		FD_CLR( sock, &mwfds );			// clear blocking
	    } // if

	    if ( FD_ISSET( sock, &mrfds ) && FD_ISSET( sock, &rfds ) ) { // waiting for data and data ?
		FD_CLR( sock, &mrfds );			// clear blocking
	    } // if
	} // if
    } // for
} // manager


int main( int argc, char *argv[] ) {
    int sock;
    socklen_t caddrlen, saddrlen;
    struct sockaddr_un server, client;
    char *tmpnm;

    switch ( argc ) {
      case 2:
	break;
      default:
	fprintf( stderr, "Usage: %s socket-name\n", argv[0] );
	exit( EXIT_FAILURE );
    } // switch

    sock = socket( AF_UNIX, SOCK_DGRAM, 0 );		// create data-gram socket
    if ( sock < 0 ) {
	perror( "Error: client socket" );
	abort();
    } // if

    if ( fcntl( sock, F_SETFL, O_NONBLOCK ) < 0 ) {	// make socket non-blocking
	perror( "Error: client non-blocking" );
	abort();
    } // if

    memset( &client, '\0', sizeof(client) );		// construct client address
    client.sun_family = AF_UNIX;
    tmpnm = tempnam( NULL, "uC++" );
    //cerr << "temp file name:\"" << tmpnm << "\"" << endl;
    strcpy( client.sun_path, tmpnm );
    caddrlen = SUN_LEN( &client );
    free( tmpnm );

    if ( bind( sock, (struct sockaddr *)&client, caddrlen ) == -1 ) {
	perror( "Error: client bind" );
	abort();
    } // if

    memset( &server, '\0', sizeof(server) );		// construct server address
    server.sun_family = AF_UNIX;
    strcpy( server.sun_path, argv[1] );
    saddrlen = SUN_LEN( &server );

    manager( sock, server, saddrlen );

    close( sock );
    if ( unlink( client.sun_path ) == -1 ) {
	perror( "Error: client unlink" );
	abort();
    } // if

    if ( wcnt != rcnt ) {
	cerr << "Error: client not all data transfered, wcnt:" << wcnt << " rcnt:" << rcnt << endl;
    } // if
} // main

// Local Variables: //
// compile-command: "g++ -g clientUNIXDGRAM.cc -o Client" //
// End: //
