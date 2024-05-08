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
#include <TCPDispatcher.hh>
#include <V1730.hh>

TCPDispatcher::TCPDispatcher(size_t _nEvents,
                             std::string _address,
                             std::string _port,
                             vector<Decoder*> _decoders)
    : Dispatcher(_nEvents, _decoders.size()),
      address(_address), port(_port),
      decoders(_decoders) {
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  getaddrinfo(_address.c_str(), _port.c_str(), &hints, &res);

  this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  int s = connect(sockfd, res->ai_addr, res->ai_addrlen);
  if (s < 0) {
    printf("TCPDispatcher: Error: Unable to connect to TCP server at %s:%s\n",
           _address.c_str(), _port.c_str());
    pthread_exit(NULL);
  }
}


TCPDispatcher::~TCPDispatcher() {
  close(sockfd);
}


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
    V1730Decoder& decoder = *((V1730Decoder*) this->decoders[i]);  // Caution!
    V1730Settings& settings = *((V1730Settings*) decoder.getSettings());  // Caution!

    // Pack intro struct
    size_t nEvents = this->evtsReady[i];

    data.nEvents = nEvents;
    snprintf(data.name, 50, "%s", settings.getIndex().c_str());
    data.type = 0xcd;
    data.bits = 14;
    data.ns_sample = 2.0;
    data.samples = static_cast<uint32_t>(settings.getRecordLength());

    memcpy(&data.counters, decoder.counters.data(), nEvents*sizeof(uint32_t));
    memcpy(&data.timetags, decoder.timetags.data(), nEvents*sizeof(uint32_t));
    memcpy(&data.exttimetags, decoder.exttimetags.data(), nEvents*sizeof(uint16_t));

    decoder.exttimetags.clear();
    decoder.timetags.clear();
    decoder.counters.clear();

    for (size_t i=0; i<decoder.nsamples.size(); i++) {
      DigitizerData::ChannelData& ch = data.channels[i];
      ch.chID = decoder.idx2chan[i];
      ch.offset = settings.getDCOffset(decoder.idx2chan[i]);
      ch.threshold = settings.getThreshold(decoder.idx2chan[i]);
      ch.dynamic_range = settings.getDynamicRange(decoder.idx2chan[i]);

      memcpy(ch.samples,
             decoder.grabs[i],
             decoder.nsamples[i]*sizeof(uint16_t)*nEvents);

      memmove(decoder.grabs[i],
              decoder.grabs[i]+nEvents*decoder.nsamples[i],
              decoder.nsamples[i]*sizeof(uint16_t)*(decoder.grabbed[i]-nEvents));

      memcpy(ch.patterns,
             decoder.patterns[i],
             sizeof(uint16_t)*nEvents);

      memmove(decoder.patterns[i],
              decoder.patterns[i]+nEvents,
              sizeof(uint16_t)*(decoder.grabbed[i]-nEvents));

      decoder.grabbed[i] -= nEvents;
    }

    // Send on socket
    int len = sizeof(DigitizerData);
    int total = 0;
    int bytesleft = len;
    int n;

    uint16_t header[2];
    header[0] = 3;  // Packet type
    header[1] = len;
    n = send(sockfd, &header, sizeof(header), 0);

    while(total < len) {
        n = send(sockfd, &data+total, bytesleft, 0);
        if (n == -1) {
          perror("TCPDispatcher: Error: client: send");
          break;
        }
        total += n;
        bytesleft -= n;
    }

    assert(total == len);
  }

  this->curCycle++;
}

