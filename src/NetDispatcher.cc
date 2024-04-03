#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cassert>
#include <vector>
#include <NetDispatcher.hh>

NetDispatcher::NetDispatcher(size_t _nEvents,
                             std::string _address, int _port,
                             vector<Decoder*> _decoders)
    : Dispatcher(_nEvents, _decoders.size()),
      address(_address), port(_port),
      decoders(_decoders) {
  struct addrinfo hints, *res;
  char sport[8];
  snprintf(sport, 8, "%i", port);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = PF_INET;
  hints.ai_socktype = SOCK_STREAM;
  getaddrinfo(address.c_str(), sport, &hints, &res);  // FIXME from config
  
  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
    perror("client: socket");
  }
  
  if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    close(sockfd);
    perror("client: connect");
  }
}


NetDispatcher::~NetDispatcher() {
  close(sockfd);
}


size_t NetDispatcher::Digest(vector<Buffer*>& buffers) {
  size_t total = 0;
  for (size_t i = 0; i < this->decoders.size(); i++) {
      size_t sz = buffers[i]->fill();
      if (sz > 0) (this->decoders)[i]->decode(*buffers[i]);
      size_t ev = (this->decoders)[i]->eventsReady();
      this->evtsReady[i] = ev;
      total += ev;
  }

  return total;
}


void NetDispatcher::Dispatch(vector<Buffer*>& buffers){
  last_time = cur_time;

  for (size_t i=0; i<this->decoders.size(); i++) {
    this->decoders[i]->pack(&data, this->evtsReady[i]);

    printf("sizeof(DigitizerData) = %li\n", sizeof(DigitizerData));
    int len = sizeof(DigitizerData);
    int total = 0;
    int bytesleft = len;
    int n;

    NetMeta meta;
    meta.packet_type = 3;
    meta.len = len;
    n = send(sockfd, &meta, sizeof(NetMeta), 0);
    printf("n=%i, size=%i\n", n, sizeof(NetMeta));
    while(total < len) {
        n = send(sockfd, &data+total, bytesleft, 0);
        printf("sent %i of %i\n", n, len);
        if (n == -1) {
          perror("client: send");
          break;
        }
        total += n;
        bytesleft -= n;
    }

    assert(total == len);
  }

  this->curCycle++;
}

