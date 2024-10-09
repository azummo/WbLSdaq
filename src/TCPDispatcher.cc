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
			     RunStart _rs,
                             vector<Decoder*> _decoders)
    : Dispatcher(_nEvents, _decoders.size()),
      address(_address), port(_port),
      rs(_rs), decoders(_decoders) {
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
  rs.first_event_id = 0;
  re.run_number = rs.run_number;
  printf("TCPDispatcher: Starting Run Number %i\n", rs.run_number);

  // Send on socket
  Send(&rs, sizeof(RunStart), RUN_START_PACKET);
}


TCPDispatcher::~TCPDispatcher() {
  std::cout << "Closing socket" << std::endl;
  re.last_event_id = last_timestamp;
  strcpy(re.last_board_name, last_board_name);
  Send(&re, sizeof(RunEnd), RUN_END_PACKET);
  close(sockfd);
}

void TCPDispatcher::End() {
  std::cout << "Closing socket" << std::endl;
  re.last_event_id = last_timestamp;
  strcpy(re.last_board_name, last_board_name);
  Send(&re, sizeof(RunEnd), RUN_END_PACKET);
  close(sockfd);
}


size_t TCPDispatcher::Digest(vector<Buffer*>& buffers){
  size_t total = 0;
  for (size_t i = 0; i < this->decoders.size(); i++) {
      size_t sz = buffers[i]->fill();
      if (sz > 0) (this->decoders)[i]->decode(*buffers[i]);
      size_t ev = (this->decoders)[i]->eventsReady();
      this->evtsReady[i] = ev;
      total += std::min(ev,(size_t)20);
  }
  return total;
}


void TCPDispatcher::Dispatch(vector<Buffer*>& buffers){
  last_time = cur_time;
  for (size_t i=0; i<this->decoders.size(); i++) {
    V1730Decoder& decoder = *((V1730Decoder*) this->decoders[i]);  // Caution!
    V1730Settings& settings = *((V1730Settings*) decoder.getSettings());  // Caution!

    // Pack intro struct

    size_t eventsRemaining = this->evtsReady[i];
    size_t totalEvents = eventsRemaining;
    size_t nEvents;
    if(eventsRemaining > 20) nEvents = 20;
    else nEvents = eventsRemaining;

    size_t ev = totalEvents-eventsRemaining;

//  Currently reads out up to 20 events from each board before moving to next
//  Could read out all events from each board before moving to next one by uncommenting this
//    while(eventsRemaining)
//    {

    data.nEvents = nEvents;
    snprintf(data.name, 50, "%s", settings.getIndex().c_str());
    data.type = 0xcd;
    data.bits = 14;
    data.ns_sample = 2.0;
    data.samples = static_cast<uint32_t>(settings.getRecordLength());

    memcpy(&data.counters, &decoder.counters[ev], nEvents*sizeof(uint32_t));
    memcpy(&data.timetags, &decoder.timetags[ev], nEvents*sizeof(uint32_t));
    memcpy(&data.exttimetags, &decoder.exttimetags[ev], nEvents*sizeof(uint16_t));

    for (size_t j=0; j<decoder.nsamples.size(); j++) {
      DigitizerData::ChannelData& ch = data.channels[j];
      ch.chID = decoder.idx2chan[j];
      ch.offset = settings.getDCOffset(decoder.idx2chan[j]);
      ch.threshold = settings.getThreshold(decoder.idx2chan[j]);
      ch.dynamic_range = settings.getDynamicRange(decoder.idx2chan[j]);

      for (size_t e=ev; e<ev+nEvents; e++) {
        memcpy(ch.samples[e],
               decoder.grabs[j]+e*decoder.nsamples[j],
               decoder.nsamples[j]*sizeof(uint16_t));
        }

      memcpy(ch.patterns,
             decoder.patterns[j],
             sizeof(uint16_t)*nEvents);
      }

    // Send on socket
    Send(&data, sizeof(DigitizerData), DAQ_PACKET);

    eventsRemaining-=nEvents;

    last_timestamp = ((uint64_t)data.exttimetags[nEvents-1] << 32) | data.timetags[nEvents-1];
    strcpy(last_board_name, data.name);

    decoder.exttimetags.erase(decoder.exttimetags.begin(), decoder.exttimetags.begin()+nEvents);
    decoder.timetags.erase(decoder.timetags.begin(), decoder.timetags.begin()+nEvents);
    decoder.counters.erase(decoder.counters.begin(), decoder.counters.begin()+nEvents);

    for (size_t j=0; j<decoder.nsamples.size(); j++) {

      memmove(decoder.grabs[j],
              decoder.grabs[j]+nEvents*decoder.nsamples[j],
              decoder.nsamples[j]*sizeof(uint16_t)*(decoder.grabbed[j]-nEvents));

      memmove(decoder.patterns[j],
              decoder.patterns[j]+nEvents,
              sizeof(uint16_t)*(decoder.grabbed[j]-nEvents));

      decoder.grabbed[j] -= nEvents;
    }
  }
  this->curCycle++;
}

void TCPDispatcher::Send(void* data, int len, int type){
// Send on socket

  int total = 0;
  int bytesleft = len;
  int n;

  uint16_t header[2];
  header[0] = type;  // Packet type
  header[1] = len;
  n = send(sockfd, &header, sizeof(header), 0);

  while(total < len) {
      n = send(sockfd, (char*)data+total, bytesleft, 0);
      if (n!= len) printf("Sent %i bytes of %i at address: %p\n", n, len, (char*)data+total);
      if (n == -1) {
        perror("TCPDispatcher: Error: client: send");
        break;
      }
      total += n;
      bytesleft -= n;
  }
  assert(total == len);
}


