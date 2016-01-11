#include "UdpRelay.h"

//void* commandThread( void *arg );

UdpRelay::UdpRelay( char *arg ) {
	// create a hash table of remote network segments
	remoteGroups = new map<const char*, UdpRelay*, ltstr>( );

	// initialize this bcast group ip and port
	char *last;
	bcastGroup = strtok_r( arg, ":", &last );
	bcastPort = atoi( strtok_r( NULL, ":", &last ) );

	if ( strncmp( bcastGroup, "0", 1 ) == 0 )
		bcastGroup = defaultBcastGroup;

	bcastPort = ( bcastPort  == 0 ) ? defaultBcastPort : bcastPort;

	// find my raw ip address
	char myname[BUFSIZE];
	bzero( myname, BUFSIZE );
	gethostname( myname, BUFSIZE );
	//cout << myname << endl;
	struct hostent *mynet = gethostbyname( myname );
	char *ipAddr = mynet->h_name;
	//cout << ipAddr << endl;
	myRawIpAddr = *( (unsigned int *)mynet->h_addr_list[0] );
	//cout << myRawIpAddr << endl;

	// nullfies remoteAddr, socket, and sd
	remoteAddr = NULL;
	sock = NULL;
	sd = -1;

	// a birth message
	printf( "UdpRelay: booted up at %s:%d\n", bcastGroup, bcastPort );

	// spawn a command thread
	pthread_create( &tid[0], NULL, commandThread, (void *)this );

	// spawn an accept thread
	pthread_create( &tid[1], NULL, acceptThread, (void *)this );

	// spawn a relay_out thread
	pthread_create( &tid[2], NULL, relayInThread, (void *)this );

	// wait for the command thread termination
	pthread_join( tid[0], NULL );
}


UdpRelay::UdpRelay( char *remoteAddr, Socket *sock, int sd, map<const char*, UdpRelay*, ltstr> *remoteGroups,
          char *bcastGroup, int bcastPort, unsigned int myRawAddr ) {
	this->remoteAddr = new char(*remoteAddr);
	this->sock = new Socket(bcastPort);
	this->sd = sd;
	this->remoteGroups = new map<const char*, UdpRelay*, ltstr> (*remoteGroups);
//	this->bcastGroup =  new char(*bcastGroup);
	this->bcastGroup = new char[strlen(bcastGroup)+1];
	bzero(this->bcastGroup, strlen(bcastGroup)+1);
	strcpy(this->bcastGroup, bcastGroup);
	this->bcastPort = bcastPort;
	this->myRawIpAddr = myRawAddr;
}


UdpRelay::~UdpRelay() {
	delete remoteAddr;
	remoteAddr = NULL;
	delete sock;
	sock = NULL;
	delete remoteGroups;
	remoteGroups = NULL;
	delete bcastGroup;
	bcastGroup = NULL;
}



//// The commandThread( ) structure is easy. Almost all its work is
//// lexically analyzing commands in a while loop:
//
void* commandThread( void *arg ) {
	UdpRelay *relay = (UdpRelay *)arg;

	while( cin ) {
	// if receiving "add", you have to start a relayOutThread( )
		string commandLine;
		getline(cin, commandLine);
		//cout << commandLine << endl;


		//Note that it returns a const char *; you aren't allowed to change the C-style string
		// returned by c_str(). If you want to process it you'll have to copy it first:
		char *cstr = new char[commandLine.length() + 1];
		strcpy(cstr, commandLine.c_str());
		char *last;
		char *command = strtok_r(cstr, " ", &last );


		if ( strncmp( command, "add", 3 ) == 0 ) {
			char *ipAddr = strtok_r( NULL, ":", &last );
			unsigned int addr = inet_addr( ipAddr );
			if ( ipAddr != NULL ) {

			// copy ipAddr to the heap
				char *rAddr = new char[strlen( ipAddr )];
				strcpy( rAddr, ipAddr );

				char *ipPort = strtok_r( NULL, ":", &last );

				if ( ipPort != NULL ) {
					// check if I have the same connection
					if ( (*(relay->remoteGroups))[rAddr] == NULL ) {
						// create a socket and establish a connection to ipAddr:port

						int port = atoi( ipPort );
						port = ( port == 0 ) ? defaultTcpPort : port;
						Socket *sock = new Socket( port );
						if ( sock != NULL ) {
							int sd = sock->getClientSocket( rAddr );
							cout << sd << endl;
							if ( sd >= 3 ) {
							// register the connection and start a thread
							relay->relayOutThreadStart( rAddr, sock, sd );
							printf( "UdpRelay: added %s:%d\n", rAddr, sd );
							continue;
							}
						}
					}
				}
			}
		} else if ( strncmp( command, "delete", 6 ) == 0 ) {
			//delete (*(relay->remoteGroups))[ipAddr];
			const char *deAddr = strtok_r( NULL, " ", &last );
			//if (relay->remoteGroups->find(deAddr))
				relay->remoteGroups->erase(deAddr);
			//else cout << "Connection not found in the connections list" << endl << endl;

		} else if ( strncmp( command, "show", 4 ) == 0 ) {
			if (relay->remoteGroups->empty())
				cout << "List of connections is empty " << endl;
			else cout << "List of connections: " << endl;
			map<const char*, UdpRelay*, ltstr>::const_iterator iter;
					for ( iter = relay->remoteGroups->begin( );
						iter != relay->remoteGroups->end( ); iter++ ) {
						const char *ip = iter->first;
						cout << ip << endl;
					}
		} else if ( strncmp( command, "help", 4 ) == 0 ){
			cout << "List of commands are: " << endl;
			cout << "add" << endl << "delete" << endl << "show"
					<< endl << "help" << endl << "quit" << endl;
		}else if ( strncmp( command, "quit", 4 ) == 0 ){
			relay->~UdpRelay();
		}
	}
}
//
//
//
//// The acceptThread( ) waits for a new connection in a while loop:
//
void* acceptThread( void * arg ) {
	// a birth message
	UdpRelay *relay = (UdpRelay *)arg;

	relay->sock = new Socket( defaultTcpPort );
	while( true ) {
		// accept a new TCP connection
		int sd = relay->sock->getServerSocket( );
		// retrieve the client ip address
		struct sockaddr_in clientAddr;
		socklen_t addrLen = sizeof( clientAddr );
		getpeername( sd, (sockaddr *)&clientAddr, &addrLen );
		char *ipAddr = inet_ntoa( clientAddr.sin_addr );
		int port = ntohs( clientAddr.sin_port );
		unsigned int addr = inet_addr( ipAddr );
		// retrieve the client ip name
		struct hostent *hptr = gethostbyaddr( &addr, sizeof( unsigned int ), AF_INET );
		char *last;
		ipAddr = hptr->h_name;
		ipAddr = strtok_r( ipAddr, ".", &last );
		char myname[BUFSIZE];
		bzero( myname, BUFSIZE );
		gethostname( myname, BUFSIZE );
		struct hostent *mynet = gethostbyname( myname );
		char *myAddr = mynet->h_name;
//		map<const char*, UdpRelay*, ltstr>::const_iterator iter;
		// check if I have the connection to the same remote client
		//for ( iter = relay->remoteGroups->begin( );
					//iter != relay->remoteGroups->end( ); iter++ ) {
//			if ( (*(relay->remoteGroups))[ipAddr] != NULL ) {
//			// if so, you may have to delete this connection. One solution is if my
//			// IP address is lexically smaller than the remote IP address
//				if ( myAddr < ipAddr ) {
//					// delete my connection
//					relay->remoteGroups->erase(ipAddr);
//					//accept this new connection
//					relay->remoteGroups->insert( std::pair<const char*, UdpRelay*> (ipAddr, relay) );
//					break;
//				}
//			}
		//}
		// if this connection is the first connection to the remote client,
		relay->remoteGroups->insert( std::pair<const char*, UdpRelay*> (ipAddr, relay) );
		cout << "UdpRelay: registered " << myAddr << endl;
		pthread_t tid;
		relay->relayOutThreadStart( ipAddr, relay->sock, sd );
	}
}

// relayInThread creates an UDP socket and waits for a local
// UDP message and to forward it to all connections
void* relayInThread( void *arg ) {
	UdpRelay *relay = (UdpRelay *)arg;

	UdpMulticast udp(relay->bcastGroup, relay->bcastPort);
	udp.getServerSocket();
	char buf[BUFSIZE], buf2[BUFSIZE];
	memset(buf, 0, sizeof(char));
	while ( true ) {
		// receive a local UDP packet
		udp.recv(buf, BUFSIZE);
		int offset = (buf[3] * 4) + 4;
		bcopy( buf + offset, buf2, sizeof (buf) - offset);
		char* packet = relay->preparePacket(buf, sizeof( buf ), buf[3]);

		// forward my local UDP message to each remote group
		map<const char*, UdpRelay*, ltstr>::const_iterator iter;
		for ( iter = relay->remoteGroups->begin( );
			iter != relay->remoteGroups->end( ); iter++ ) {
			// forward a UDP packet to each connection.
			size_t nWritten = write(iter->second->sd, packet, BUFSIZE);

			struct sockaddr_in clientAddr;
			socklen_t addrLen = sizeof( clientAddr );
			getpeername( iter->second->sd, (sockaddr *)&clientAddr, &addrLen );
			char *ipAddr = inet_ntoa( clientAddr.sin_addr );
			int port = ntohs( clientAddr.sin_port );
			unsigned int addr = inet_addr( ipAddr );
			// retrieve the client ip name
			struct hostent *hptr = gethostbyaddr( &addr, sizeof( unsigned int ), AF_INET );
			char *last;
			ipAddr = hptr->h_name;
			ipAddr = strtok_r( ipAddr, ".", &last );
			char myname[BUFSIZE];
			bzero( myname, BUFSIZE );
			gethostname( myname, BUFSIZE );
			struct hostent *mynet = gethostbyname( myname );
			char *myAddr = mynet->h_name;

			cout << "UdpRelay: relay: " << buf2 << " to [" << ipAddr << ":" << relay->bcastPort << "]" <<  endl;
		}
	}
}

// istantiates a new copy of RelayUdp and start the relayOut Thread on it
void UdpRelay::relayOutThreadStart( char *ipAddr, Socket *sock, int sd ) {
  // register this socket
  UdpRelay *relayOut = new UdpRelay( ipAddr, sock, sd, remoteGroups, bcastGroup, bcastPort, myRawIpAddr );
  (*remoteGroups)[ipAddr] = relayOut;
  printf( "UdpRelay: registered %s\n", ipAddr);

  // start a thread
  pthread_t tid;
  pthread_create( &tid, NULL, relayOutThread, (void *)relayOut );
}


// relayOutThread listens to the outside( other segments routers)
// and transmits the coming UDP packets to the local segment
void* relayOutThread( void * arg ) {
	UdpRelay *relay = (UdpRelay *)arg;
	int clientSd = relay->sd;
	char buf[BUFSIZE], buf2[BUFSIZE];
	memset(buf, 0, sizeof(char));

	while (true) {
		size_t nRead = read( clientSd, buf, BUFSIZE );

		for (int i = 4; i < (buf[3]+3); i++) {
			if( relay->myRawIpAddr == buf[i])
				return NULL;
		}

		int offset = (buf[3] * 4) + 8;
		bcopy( buf + offset, buf2, sizeof (buf) - offset);

		struct sockaddr_in clientAddr;
		socklen_t addrLen = sizeof( clientAddr );
		getpeername( clientSd, (sockaddr *)&clientAddr, &addrLen );
		char *ipAddr = inet_ntoa( clientAddr.sin_addr );
		int port = ntohs( clientAddr.sin_port );
		unsigned int addr = inet_addr( ipAddr );
		// retrieve the client ip name
		struct hostent *hptr = gethostbyaddr( &addr, sizeof( unsigned int ), AF_INET );
		char *last;
		ipAddr = hptr->h_name;
		ipAddr = strtok_r( ipAddr, ".", &last );
		char myname[BUFSIZE];
		bzero( myname, BUFSIZE );
		gethostname( myname, BUFSIZE );
		struct hostent *mynet = gethostbyname( myname );
		char *myAddr = mynet->h_name;
		cout << "UdpRelay: received: "<< nRead << " bytes from [" << ipAddr << ":" <<
				relay->bcastPort  << "]" << " msg= " << buf2 << endl;
		char* packet = relay->preparePacket(buf, BUFSIZE, buf[3]);
		UdpMulticast udp( relay->bcastGroup, defaultBcastPort);
		udp.getClientSocket();
		cout << "UdpRelay: broadcast buf[" << nRead + 4 << "] to " << relay->bcastGroup
				<< ":" << relay->bcastPort  << endl;
		udp.multicast(packet);
	}
}



char *UdpRelay::preparePacket( char buf[], int orgSize, int hops ) {
  // add header
  char *packet = new char[orgSize + 4];
  bzero( packet, orgSize + 4 );
  //          alpha            beta             gamma            hops
  packet[0] = -32; packet[1] = -31; packet[2] = -30; packet[3] = hops;
  int formerIpAddressLen = ( hops - 1 ) * 4;

  // all former udp_relay raw IP addresses
  bcopy( buf + 4, packet + 4, formerIpAddressLen );

  // copy my raw ip address
  bcopy( (void *)&myRawIpAddr, packet + 4 + formerIpAddressLen, 4 );

  // copy the origianl payLoad
  int offset = 4 + formerIpAddressLen;
  bcopy( buf + offset, packet + offset + 4, orgSize - offset );

  return packet;
}











