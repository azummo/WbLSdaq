// ejc

#ifndef Dispatch_hh
#define Dispatch_hh

#include <vector>
#include <H5Cpp.h>
#include <Buffer.hh>
#include <Digitizer.hh>
#include <RunType.hh>
#include <Decode.hh>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

using namespace std;
using namespace H5;

extern bool stop;
bool dispatch_running;

extern void int_handler(int x);

typedef struct {
    vector<Buffer*> *buffers;
    vector<Decoder*> *decoders;
//  vector<Dispatcher*> *dispatchers;
    pthread_mutex_t *iomutex;
    pthread_cond_t *newdata;
    string config;
    RunType *runtype;
} dispatch_thread_data;

void *dispatch_thread(void *_data) {
    signal(SIGINT,int_handler);
    dispatch_thread_data* data = (dispatch_thread_data*)_data;

    string path = "/home/eos/ejc/socket/socket";
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    char _path[256];
    sprintf(_path, "%s", path.c_str());
    struct sockaddr_un saddr;
    saddr.sun_family = AF_UNIX;
    sprintf(saddr.sun_path, "%s", path.c_str());
    if (connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr)) < 0){
         cerr << "error during socket connection";
         pthread_exit(NULL);
    }

    vector<size_t> evtsReady(data->buffers->size());
    data->runtype->begin();
    try {
        dispatch_running = true;
        while (dispatch_running) {
            pthread_mutex_lock(data->iomutex);
            for (;;) {
                bool found = stop;
                for (size_t i = 0; i < data->buffers->size(); i++) {
                    found |= (*data->buffers)[i]->fill() > 0;
                }
                if (found) break;
                pthread_cond_wait(data->newdata,data->iomutex);
            }
            
            size_t total = 0;
            for (size_t i = 0; i < data->buffers->size(); i++) {
                size_t sz = (*data->buffers)[i]->fill();
//              if (sz > 0) (*data->dispatchers)[i]->dispatch(*(*data->buffers)[i]);
//              if (sz > 0) (*data->decoders)[i]->decode(*(*data->buffers)[i]);
//              size_t ev = (*data->decoders)[i]->eventsReady();
//              evtsReady[i] = ev;
                total += sz;
            }
            
            if (stop && total == 0) {
                dispatch_running = false;
            } else if (stop || data->runtype->writeout(evtsReady) || total) {
                Exception::dontPrint();

                // FIXME should it be the path or the actual socket?
                // I think the latter...
//              string path = data->runtype->socket();
                cout << "Streaming data to " << path.c_str() << endl;
                // ejc
                // connect to socket and write into stream
//              H5File file(fname, H5F_ACC_TRUNC);
//              data->runtype->write(file);
//                
//              DataSpace scalar(0,NULL);
//              Group root = file.openGroup("/");

        // TODO this should just be a dump into socket
                for (size_t i = 0; i < data->buffers->size(); i++) {
//                  (*data->dispatchers)[i]->writeOut(file,evtsReady[i]);
                    size_t writeable = (*data->buffers)[i]->fill();
                    size_t written = send(sockfd,
                                          (*data->buffers)[i]->rptr(),
                                          writeable,
                                          0);
                    (*data->buffers)[i]->dec(written);
                    if (written < writeable){
                        cerr << "incomplete write!" << endl;
                        exit(1);
                    }
                }

                dispatch_running = data->runtype->keepgoing();
            }
            pthread_mutex_unlock(data->iomutex);
        }
        stop = true;
    } catch (runtime_error &e) {
        pthread_mutex_unlock(data->iomutex);
        stop = true;
        pthread_mutex_lock(data->iomutex);
        cout << "Dispatch thread aborted: " << e.what() << endl;
        pthread_mutex_unlock(data->iomutex);
    }
    pthread_exit(NULL);
}

#endif

