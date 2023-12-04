#include <TCPDispatcher.hh>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>

TCPDispatcher::TCPDispatcher(size_t _nEvents,
                             int _port,
                             vector<Decoder*> _decoders)
    : Dispatcher(_nEvents, _decoders.size()),
      port(_port),
      decoders(_decoders) {
  this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = INADDR_ANY;
  saddr.sin_port = htons(port);
  int s = connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr));
  if (s < 0) {
    printf("oh no an error %i\n", s);
    pthread_exit(NULL);
  }
}


TCPDispatcher::~TCPDispatcher() {}


size_t TCPDispatcher::Digest(vector<Buffer*>& buffers){
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


void TCPDispatcher::Dispatch(vector<Buffer*>& buffers){
  last_time = cur_time;
    
  for (size_t i=0; i<this->decoders.size(); i++) {
    this->decoders[i]->pack(&data, this->evtsReady[i]);

    char* buf = (char*)(&data);
    int total = 0;
    int rem = sizeof(DigitizerData);
    int n;

    while(total < (int)sizeof(DigitizerData)) {
        n = send(sockfd, buf+total, rem, 0);
        if (n == -1) { break; }
        total += n;
        rem -= n;
    }

    //size_t written = send(sockfd, (void*)(&data), sizeof(DigitizerData), 0);

    printf("wrote out %i / %li bytes.\n", total, sizeof(DigitizerData));
  }

  this->curCycle++;
}

