#include <ZMQDispatcher.hh>
#include <zmq.hpp>

ZMQDispatcher::ZMQDispatcher(size_t _nEvents,
                             std::string _address,
                             vector<Decoder*> _decoders)
    : Dispatcher(_nEvents, _decoders.size()),
      address(_address),
      decoders(_decoders) {
  context = new zmq::context_t(1);
  socket = new zmq::socket_t(*context, ZMQ_PUB);
  socket->bind(address.c_str());
}


ZMQDispatcher::~ZMQDispatcher() {}


size_t ZMQDispatcher::Digest(vector<Buffer*>& buffers){
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


void ZMQDispatcher::Dispatch(vector<Buffer*>& buffers){
  last_time = cur_time;
    
  for (size_t i=0; i<this->decoders.size(); i++) {
    this->decoders[i]->pack(&data, this->evtsReady[i]);
    zmq::const_buffer buf((void*)(&data), sizeof(DigitizerData));
    socket->send(buf);
  }

  this->curCycle++;
}

