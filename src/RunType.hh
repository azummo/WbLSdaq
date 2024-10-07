// ejc

#ifndef RunType__hh
#define RunType__hh

#include <string>
#include <H5Cpp.h>

using namespace std;
using namespace H5;

class RunType {
    public:
        virtual ~RunType(){};

        //called just before readout begins
        virtual void begin() = 0;
        
        //called after data is written to add more data or prepare for next file
        virtual bool keepgoing(size_t total) = 0;
};

// Gets fixed numbers of events, optionally splitting into multiple files (repeating)
// nEvents is the number of events to grab for all cards (0 for continuious run)
// nRepeat is the number of times to repeat (0 for once, 1 for once, N for multiple)
class NEventsRun : public RunType {
    protected:
        size_t nRepeat, curCycle;
        struct timespec cur_time, last_time;

    public: 
        NEventsRun(size_t _nRepeat = 0) : 
            nRepeat(_nRepeat), 
            curCycle(0) { }

        virtual ~NEventsRun() {
            /* */
        }

        virtual void begin() {
            clock_gettime(CLOCK_MONOTONIC,&last_time);
        }

        virtual bool keepgoing(size_t total) {
            if (nRepeat > 0) {
                last_time = cur_time;
                curCycle++;
                return curCycle != nRepeat;
            } else {
                return false;
            }
        }
};

// Gets total numbers of events
// nTotal is the number of events to grab for all cards (0 for continuious run)
class TotalEventsRun : public RunType {
    protected:
        size_t nEvents, nTotal;
        struct timespec cur_time, last_time;

    public:
        TotalEventsRun(size_t _nTotal = 0) :
            nTotal(_nTotal) { }

        virtual ~TotalEventsRun() {
            /* */
        }

        virtual void begin() {
            clock_gettime(CLOCK_MONOTONIC,&last_time);
        }

        virtual bool keepgoing(size_t total) {
            if (nTotal > 0) {
                last_time = cur_time;
                nEvents+=total;
                return nEvents <= nTotal;
            } else {
                return true;
            }
        }
};

/*
//Runs for a specified amount of time (seconds) splitting files by a certain 
//number of events (0 means all one file - but watch your buffers). 
//Assumes event rate is faster than runtime, i.e. the acquisition will stop on 
//the first event after time runs out.
class TimedRun : public RunType {
    protected:
        string basename;
        size_t runtime, evtsPerFile, curCycle;
        double total;
        struct timespec cur_time, last_time, begin_time;

    public: 
        TimedRun(string _basename, size_t _runtime, size_t _evtsPerFile) : 
            basename(_basename),
            runtime(_runtime), 
            evtsPerFile(_evtsPerFile), 
            curCycle(0) { }

        virtual ~TimedRun() {
        }

        virtual void begin() {
            clock_gettime(CLOCK_MONOTONIC,&begin_time);
            clock_gettime(CLOCK_MONOTONIC,&last_time);
        }

        virtual bool writeout(std::vector<size_t> &evtsReady) {
            total = 0.0;
            bool writeout = evtsPerFile > 0;
            for (size_t i = 0; i < evtsReady.size(); i++) {
                total += evtsReady[i];
                if (evtsReady[i] < evtsPerFile) writeout = false;
            }
            total /= evtsReady.size();
            
            if (evtsPerFile > 0) cout << "Cycle " << curCycle+1 << endl;
            
            clock_gettime(CLOCK_MONOTONIC,&cur_time);
            double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
            cout << "Avg rate " << total/time_int << " Hz" << endl;
            
            time_int = (cur_time.tv_sec - begin_time.tv_sec)+1e-9*(cur_time.tv_nsec - begin_time.tv_nsec);
            if (time_int >= runtime) writeout = true;
            
            return writeout;
        }

        virtual string fname() {
            if (evtsPerFile > 0) {
                return basename + "." + to_string(curCycle);
            } else {
                return basename;
            }
        }

        virtual bool keepgoing() {
            if (evtsPerFile > 0) curCycle++;
            double time_int = (cur_time.tv_sec - begin_time.tv_sec)+1e-9*(cur_time.tv_nsec - begin_time.tv_nsec);
            return time_int < runtime;
        }
};
*/

class ManualRun : public RunType {
    protected:
        size_t curCycle;
        struct timespec cur_time, last_time;

    public:
        ManualRun():
            curCycle(0) { }

        virtual ~ManualRun() {
            /* */
        }

        virtual void begin() {
            clock_gettime(CLOCK_MONOTONIC,&last_time);
        }

        virtual bool keepgoing(size_t total) {
            curCycle++;
            return true;
        }
};

#endif
