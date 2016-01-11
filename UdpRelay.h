#ifndef _UDPRELAY_H_
#define _UDPRELAY_H_

#include "Socket.h"
#include "UdpMulticast.h"
#include <pthread.h>
#include <cstring>
#include <string.h>
#include <string>
#include <map>
#define defaultBcastGroup "237.255.255.255"
#define defaultBcastPort 64791
#define defaultTcpPort   64791
#define MSGSIZE 1024
//#define BUFSIZE 256
#define HOP_MAX 10

using namespace std;

void *commandThread( void *arg );
void *acceptThread( void *arg );
void *relayInThread( void *arg );
void *relayOutThread( void *arg );

struct ltstr {
  bool operator( )( const char* s1, const char* s2 ) const {
    return strcmp( s1, s2 ) < 0;
  }
};

class UdpRelay {
public:
  UdpRelay( char *arg );
  UdpRelay( char *remoteAddr, Socket *sock, int sd,
            map<const char*, UdpRelay*, ltstr> *remoteGroups,
            char *bcastGroup, int bcastPort, unsigned int myRawAddr );
  ~UdpRelay( );
private:
  pthread_t tid[3];
  char *bcastGroup;
  int bcastPort;
  char *remoteAddr;
  Socket *sock;
  int sd;
  unsigned int myRawIpAddr;
  map<const char*, UdpRelay*, ltstr> *remoteGroups;

  bool foundMyAddress( char buf[], char &hops );
  char *preparePacket( char buf[], int org, int hops );
  char *getSrcIpAddr( struct sockaddr_in *addr );
  void broadcast( char *buf, int size );
  void relayOutThreadStart( char *ipAddr, Socket *sock, int id );

  friend void *commandThread( void *arg );
  friend void *acceptThread( void *arg );
  friend void *relayInThread( void *arg );
  friend void *relayOutThread( void *arg );
};

#endif
