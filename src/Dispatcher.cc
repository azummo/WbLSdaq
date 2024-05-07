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

bool Dispatcher::Ready(bool &print, string path){
    double rate = 0.0;
    bool rv = this->nEvents > 0;
    for (size_t i = 0; i < this->evtsReady.size(); i++) {
        rate += this->evtsReady[i];
    }
    if (rate < nEvents) rv = false;
    rate /= evtsReady.size();

    clock_gettime(CLOCK_MONOTONIC,&cur_time);
    double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
    rate /= time_int;

    if(curCycle > 0){
	print = true;
    }
    if(print && path != previous_path){
        cout << "Cycle " << curCycle << ", Trig. Rate: " << rate << ", nEV: " << this->nEvents << endl;
	print = false;
    }

    previous_path = path;

    return rv;
}
