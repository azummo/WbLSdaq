// ejc

#include <Dispatcher.hh>

Dispatcher::Dispatcher(){
    /* */
}

Dispatcher::Dispatcher(size_t _nEvents, size_t _nStreams){
    this->curCycle = 0;
    this->nEvents = _nEvents;
    this->evtsReady = vector<size_t>(_nStreams);
}

Dispatcher::~Dispatcher(){
    /* */
}

bool Dispatcher::Ready(){
    double rate = 0.0;
    bool rv = this->nEvents > 0;
    for (size_t i = 0; i < this->evtsReady.size(); i++) {
        rate += this->evtsReady[i];
        //if (this->evtsReady[i] < nEvents) rv = false;
        std::cout << "evtsReady[" << i << "] " << evtsReady[i] << std::endl;
    }
    if (rate < nEvents) rv = false;
    rate /= evtsReady.size();
    
    cout << "Cycle " << curCycle+1 << endl;
    
    clock_gettime(CLOCK_MONOTONIC,&cur_time);
    double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
    rate /= time_int;
    cout << "Avg rate " << rate << " Hz" << endl;
    
    std::cout << "nEvents " << this->nEvents << ", rv " << rv << std::endl;
    return rv;
}
