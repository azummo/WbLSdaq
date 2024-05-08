// ejc

#ifndef Readout_hh
#define Readout_hh

#include <iostream>
#include <vector>
#include <CAENCONETNetwork.hh>
#include <CONETNetwork.hh>
#include <VMEBridge.hh>
#include <Dispatcher.hh>
#include <TCPDispatcher.hh>
#include <LegacyHDF5Dispatcher.hh>
#include <SocketDispatcher.hh>
#include <SoftwareTrigger.hh>
#include <RunDB.hh>
#include <RunType.hh>

using namespace std;

bool stop;
pthread_mutex_t stopmutex;
bool readout_running;
extern bool decode_running;

typedef struct {
    RunDB db;
    RunTable run;
    int rv;
    bool stop;
    pthread_mutex_t mutex;
} readout_thread_data;

void *readout(void *_data){
    RunDB db = ((readout_thread_data*) _data)->db;
    RunTable run = ((readout_thread_data*) _data)->run;
    readout_thread_data* rdata = (readout_thread_data*) _data;
    rdata->rv = -1;

    const string runtypestr = run["runtype"].cast<string>();
    RunType *runtype = NULL;
    size_t eventBufferSize = 0;
    if (run.isMember("event_buffer_size")) {
        eventBufferSize = run["event_buffer_size"].cast<int>();
    } 
    if (runtypestr == "nevents") {
        cout << "Setting up an event limited run..." << endl;
        const string outfile = run["outfile"].cast<string>();
        const int nEvents = run["events"].cast<int>();
        int nRepeat;
        if (run.isMember("repeat_times")) {
            nRepeat = run["repeat_times"].cast<int>();
        } else {
            nRepeat = 0;
        }
        runtype = new NEventsRun(nRepeat);
        if (!eventBufferSize) eventBufferSize = (size_t)(nEvents*1.5);
    }
    else if (runtypestr == "manual"){
        cout << "Setting up a manually-stopped  run..." << endl;
        const string outfile = run["outfile"].cast<string>();
        runtype = new ManualRun();
    }
    /*else if (runtypestr == "timed") {
        cout << "Setting up a time limited run..." << endl;
        const string outfile = run["outfile"].cast<string>();
        int evtsPerFile;
        if (run.isMember("events_per_file")) {
            evtsPerFile = run["events_per_file"].cast<int>();
            if (evtsPerFile == 0) {
                cout << "Cannot do a timed run all in one file - events_per_file must be nonzero" << endl;
                pthread_exit(&(((readout_thread_data*) _data)->rv));
            }
        } else {
            evtsPerFile = 1000; //we need a well defined buffer amount
        }
        runtype = new TimedRun(outfile,run["runtime"].cast<int>(),evtsPerFile);
        if (!eventBufferSize) eventBufferSize = (size_t)(evtsPerFile*1.5);
    }*/

    if (!runtype){
        cout << "Unknown runtype: " << runtypestr << endl;
        pthread_exit(&(((readout_thread_data*) _data)->rv));
    }

    cout << "Using " << eventBufferSize << " event buffers." << endl;
    
    //Every run has these options
    const int temptime = run["check_temps_every"].cast<int>();
    bool config_only = false;
    if (run.isMember("config_only")) {
        config_only = run["config_only"].cast<bool>();
    }
    
    cout << "Grabbing V1742 calibration..." << endl;
    
    //This has to be done before using the CANEVME library due to bugs in the
    //CAENDigitizer library... so hack it in here.
    vector<RunTable> v1742s = db.getGroup("V1742");
    vector<V1742Settings*> v1742settings;
    vector<V1742calib*> v1742calibs;
    for (size_t i = 0; i < v1742s.size(); i++) {
        RunTable &tbl = v1742s[i];
        cout << "* V1742 - " << tbl.getIndex() << endl;
        V1742Settings *stngs = new V1742Settings(tbl,db);
        v1742settings.push_back(stngs);
        v1742calibs.push_back(V1742::staticGetCalib(stngs->sampleFreq(),0,tbl["base_address"].cast<int>()));
    }

    map<string, BoardCommManager*> communications;
    vector<RunTable> links = db.getGroup("Communication");
    if (links.size() > 0) cout << "Opening communication links..." << endl;
    for (size_t i =0; i < links.size(); i++){
        RunTable &tbl = links[i];
        cout << "\t" << tbl["index"].cast<string>() << endl;
        BoardCommManager* link;
        if (tbl["type"].cast<string>() == "VMEBridge") {
            // from the device having swapped to _1, have tracked this down
            // as a bug?
            const int linknum = tbl["link_num"].cast<int>();
            link = new VMEBridge(0,linknum);
        }
        else if (tbl["type"].cast<string>() == "CONETNetwork") {
            const int linknum = tbl["link_num"].cast<int>();
            link = new CONETNetwork(linknum);
        }
        else if (tbl["type"].cast<string>() == "CAENCONETNetwork") {
            const int linknum = tbl["link_num"].cast<int>();
            link = new CAENCONETNetwork(linknum);
        }
        else {
            stringstream err;
            err << "Unknown communcation channel type: " << tbl["type"].cast<string>();
            throw runtime_error(err.str());
        }
        communications[tbl["index"].cast<string>()] = link;
    }
    
    vector<V65XX*> hvs;
    vector<RunTable> v65XXs = db.getGroup("V65XX");
    if (v65XXs.size() > 0) cout << "Setting up V65XX HV..." << endl;
    for (size_t i = 0; i < v65XXs.size(); i++) {
        RunTable &tbl = v65XXs[i];
        cout << "\t" << tbl["index"].cast<string>() << endl;
        BoardCommManager& bridge = *(communications[tbl["communication"].cast<string>()]);
        hvs.push_back(new V65XX(bridge,tbl["base_address"].cast<int>()));
        hvs.back()->set(tbl);
    }
    
    vector<LeCroy6Zi*> lescopes;
    vector<RunTable> lecroy6zis = db.getGroup("LECROY6ZI");
    if (lecroy6zis.size() > 0) cout << "Setting up LeCroy6Zi..." << endl;
    for (size_t i = 0; i < lecroy6zis.size(); i++) {
        RunTable &tbl = lecroy6zis[i];
        cout << "\t" << tbl["index"].cast<string>() << endl;
        RemoteCommunication* remote = NULL;
        if (tbl.isMember("host"))
            remote = new EthernetCommunication(tbl["host"].cast<string>(),tbl["port"].cast<int>(),tbl["timeout"].cast<double>());
        else if (tbl.isMember("path"))
            remote = new FileCommunication(tbl["path"].cast<string>());
        else
            throw runtime_error("Remote communication must be ethernet- or file-baed");
        lescopes.push_back(new LeCroy6Zi(remote));
        lescopes.back()->reset();
//      lescopes.back()->checklast();
        lescopes.back()->recall(tbl["load"].cast<int>());
//      lescopes.back()->checklast();
    }
    
    cout << "Setting up digitizers..." << endl;
    
    vector<DigitizerSettings*> settings;
    vector<Digitizer*> digitizers;
    vector<Buffer*> buffers;
    vector<Decoder*> decoders;

/*
    vector<RunTable> v1730wavedumps = db.getGroup("V1730wavedump");
    for (size_t i = 0; i < v1730wavedumps.size(); i++) {
        RunTable &tbl = v1730wavedumps[i];
        cout << "* V1730wavedump - " << tbl.getIndex() << endl;
        V1730_wavedumpSettings *stngs = new V1730_wavedumpSettings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1730_wavedump(bridge,tbl["base_address"].cast<int>()));
        ((V1730_wavedump*)digitizers.back())->stopAcquisition();
        ((V1730_wavedump*)digitizers.back())->calib();
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs)) return -1;
        // decoders need settings after programming
        decoders.push_back(new V1730_wavedumpDecoder(eventBufferSize,*stngs));
    }

    vector<RunTable> v1730dummys = db.getGroup("V1730dummy");
    for (size_t i = 0; i < v1730dummys.size(); i++) {
        RunTable &tbl = v1730dummys[i];
        cout << "* V1730dummy - " << tbl.getIndex() << endl;
        V1730_dummySettings *stngs = new V1730_dummySettings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1730_dummy(bridge,tbl["base_address"].cast<int>()));
        ((V1730_dummy*)digitizers.back())->stopAcquisition();
        ((V1730_dummy*)digitizers.back())->calib();
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs)) return -1;
        // decoders need settings after programming
        decoders.push_back(new V1730_dummyDecoder(eventBufferSize,*stngs));
    }
*/

    vector<RunTable> v1730dpps = db.getGroup("V1730dpp");
    for (size_t i = 0; i < v1730dpps.size(); i++) {
        RunTable &tbl = v1730dpps[i];
        BoardCommManager& bridge = *(communications[tbl["communication"].cast<string>()]);
        cout << "* V1730dpp - " << tbl.getIndex() << endl;
        V1730_DPPSettings *stngs = new V1730_DPPSettings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1730_DPP(bridge,tbl["base_address"].cast<int>()));
        ((V1730_DPP*)digitizers.back())->stopAcquisition();
        ((V1730_DPP*)digitizers.back())->calib();
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs))
            pthread_exit(&(((readout_thread_data*) _data)->rv));
        // decoders need settings after programming
        decoders.push_back(new V1730_DPPDecoder(eventBufferSize,*stngs));
    }
    
    vector<RunTable> v1730s = db.getGroup("V1730");
    for (size_t i = 0; i < v1730s.size(); i++) {
        RunTable &tbl = v1730s[i];
        BoardCommManager& bridge = *(communications[tbl["communication"].cast<string>()]);
        cout << "* V1730 - " << tbl.getIndex() << endl;
        V1730Settings *stngs = new V1730Settings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1730(bridge,tbl["base_address"].cast<int>()));
        ((V1730*)digitizers.back())->stopAcquisition();
        ((V1730*)digitizers.back())->calib();
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs))
            pthread_exit(&(((readout_thread_data*) _data)->rv));
        // decoders need settings after programming
        decoders.push_back(new V1730Decoder(eventBufferSize,*stngs));
    }
    
    for (size_t i = 0; i < v1742s.size(); i++) {
        RunTable &tbl = v1742s[i];
        BoardCommManager& bridge = *(communications[tbl["communication"].cast<string>()]);
        cout << "* V1742 - " << tbl.getIndex() << endl;
        V1742Settings *stngs = v1742settings[i];
        settings.push_back(stngs);
        V1742 *card = new V1742(bridge,tbl["base_address"].cast<int>());
        card->stopAcquisition();
        digitizers.push_back(card);
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        if (!digitizers.back()->program(*stngs))
            pthread_exit(&(((readout_thread_data*) _data)->rv));
        // decoders need settings after programming
        decoders.push_back(new V1742Decoder(eventBufferSize,v1742calibs[i],*stngs)); 
    }

    if (!db.groupExists("Dispatch")){
        cerr << "error: must specify a Dispatch block in config" << endl;
        pthread_exit(NULL);
    }
    RunTable disptbl = db.getGroup("Dispatch")[0];
    string dispstr = disptbl["type"].cast<string>();
    Dispatcher* dispatcher = NULL;
    if (dispstr == "legacy_hdf5"){
        int nEvents = run["events"].cast<int>();
        string basename = run["outfile"].cast<string>();
        dispatcher = new LegacyHDF5Dispatcher(nEvents, basename, decoders);
    }
    else if (dispstr == "socket"){
        int nEvents = run["events"].cast<int>();
        string path = run["outfile"].cast<string>();
        dispatcher = new SocketDispatcher(nEvents, path, buffers.size());
    }
    else if (dispstr == "tcp"){
        int nEvents = run["events"].cast<int>();
	std::string address = disptbl["address"].cast<string>();
        std::string port = disptbl["port"].cast<string>();
        dispatcher = new TCPDispatcher(nEvents, address, port, decoders);
    }
    if (!dispatcher){
        cerr << "error: dispatcher type "
             << dispstr
             << " is not supported"
             << endl;
        pthread_exit(NULL);
    }

    size_t arm_last = 0;
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (run.isMember("arm_last") && settings[i]->getIndex() == run["arm_last"].cast<string>()) 
            arm_last = i;
    }

    cout << "Waiting for HV to stabilize..." << endl;

    while (!stop) {
        bool busy = false;
        bool warning = false;
        for (size_t i = 0; i < hvs.size(); i++) {
             busy |= hvs[i]->isBusy();
             warning  |= hvs[i]->isWarning();
        }
        if (!busy) break;
        if (warning) {
            cout << "HV reports issues, aborting run..." << endl;
            pthread_exit(&(rdata->rv));
        }
        pthread_mutex_lock(&rdata->mutex);
        if (rdata->stop)
            stop = true;
        pthread_mutex_unlock(&rdata->mutex);
        usleep(1000000);
    }

    cout << "Starting acquisition..." << endl;

    pthread_mutex_t iomutex;
    pthread_cond_t newdata;
    pthread_mutex_init(&iomutex,NULL);
    pthread_cond_init(&newdata, NULL);
    vector<uint32_t> temps;

    /* better software-triggering below
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (i == arm_last) continue;
        digitizers[i]->startAcquisition();
        if (run.isMember("soft_trig") && settings[i]->getIndex() == run["soft_trig"].cast<string>()) {
            cout << "Software triggering " << settings[i]->getIndex() << endl;
            digitizers[i]->softTrig();
        }
    }
    if (digitizers.size() > 0) {
        digitizers[arm_last]->startAcquisition();
        if (run.isMember("soft_trig") && settings[arm_last]->getIndex() == run["soft_trig"].cast<string>()) {
            cout << "Software triggering " << settings[arm_last]->getIndex() << endl;
            digitizers[arm_last]->softTrig();
        }
    }
    */
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (i == arm_last) continue;
        digitizers[i]->startAcquisition();
        for (size_t j = 0 ; j < run["soft_trig"].getArraySize(); j++){
            if (run.isMember("soft_trig") && settings[i]->getIndex() == run["soft_trig"][j].cast<string>()) {
                cout << "Software triggering " << settings[i]->getIndex() << endl;
                digitizers[i]->softTrig();
            }
        }
    }
    if (digitizers.size() > 0) {
        digitizers[arm_last]->startAcquisition();
        for (size_t j = 0 ; j < run["soft_trig"].getArraySize(); j++){
            if (run.isMember("soft_trig") && settings[arm_last]->getIndex() == run["soft_trig"][j].cast<string>()) {
                cout << "Software triggering " << settings[arm_last]->getIndex() << endl;
                digitizers[arm_last]->softTrig();
            }
        }
    }
    for (size_t i = 0; i < lescopes.size(); i++) {
        lescopes[i]->normal();
        delete lescopes[i]; //get rid of these until the acquisition is done
        lescopes[i] = NULL;
    }
    
    dispatch_thread_data data;
    data.buffers = &buffers;
    data.dispatcher = dispatcher;
    data.iomutex = &iomutex;
    data.newdata = &newdata;
    data.runtype = runtype;
//  { //copy entire config as-is to be saved in each file
//      std::ifstream file(argv[1]);
//      std::stringstream buf;
//      buf << file.rdbuf();
//      data.config = buf.str();
//  }
    pthread_t decode;
//  pthread_create(&decode,NULL,&decode_thread,&data);
    pthread_create(&decode,NULL,&dispatch_thread,&data);

    software_trigger_thread_data st_data;
    st_data.settings = &settings;
    st_data.digitizers = &digitizers;
    st_data.run = run;
    pthread_t software_trigger;
    pthread_create(&software_trigger,NULL,&software_trigger_thread,&st_data);

    struct timespec last_temp_time, cur_time;
    clock_gettime(CLOCK_MONOTONIC,&last_temp_time);
    
    if (config_only) stop = true;

    try { 
        readout_running = true;
        while (readout_running && !stop) {
            //Digitizer loop
            for (size_t i = 0; i < digitizers.size() && !stop; i++) {
                Digitizer *dgtz = digitizers[i];
                if (dgtz->readoutReady()) {
                    buffers[i]->inc(dgtz->readoutBLT(buffers[i]->wptr(),buffers[i]->free()));
                    usleep(100);
                    pthread_cond_signal(&newdata);
                }
                if (!dgtz->acquisitionRunning()) {
                    pthread_mutex_lock(&iomutex);
                    cout << "Digitizer " << settings[i]->getIndex() << " aborted acquisition!" << endl;
                    pthread_mutex_unlock(&iomutex);
                    stop = true;
                }
            }
            
            //Temperature check
            clock_gettime(CLOCK_MONOTONIC,&cur_time);
            if (cur_time.tv_sec-last_temp_time.tv_sec > temptime) {
                pthread_mutex_lock(&iomutex);
                last_temp_time = cur_time;
                cout << "Temperature check...";
                bool overtemp = false;
                int warning_count = 0;
		for (size_t i = 0; i < digitizers.size() && !stop; i++) {
                    overtemp |= digitizers[i]->checkTemps(temps,84);
                    for (size_t t = 1; t < temps.size(); t++){
	                if(temps[t] > 65){
		            warning_count += 1;
			}
	            }
                }
		if(warning_count > 0){
		    cout << "WARNING: " << warning_count << " temps above 65 deg." << endl;
		}
		else{
	            cout << "All below 65 deg." << endl;
		}
                if (overtemp) {
                    cout << "Overtemp! Aborting readout." << endl;
                    stop = true;
                }
                pthread_mutex_unlock(&iomutex);
            }

            // check if main thread has instructed to stop
            pthread_mutex_lock(&rdata->mutex);
            if (rdata->stop)
                stop = true;
            pthread_mutex_unlock(&rdata->mutex);
        } 
        readout_running = false;
    } catch (exception &e) {
        readout_running = false;
        pthread_mutex_unlock(&iomutex);
        pthread_mutex_lock(&iomutex);
        cout << "Readout thread aborted: " << e.what() << endl;
        pthread_mutex_unlock(&iomutex);
    }
    
    stop = true;
    pthread_mutex_lock(&iomutex);
    cout << "Stopping acquisition..." << endl;
    pthread_mutex_unlock(&iomutex);
    
    for (size_t i = 0; i < lecroy6zis.size(); i++) {
        RunTable &tbl = lecroy6zis[i];
        try { //want to make sure this doesn't ever cause a crash
            RemoteCommunication* remote = NULL;
            if (tbl.isMember("host"))
                remote = new EthernetCommunication(tbl["host"].cast<string>(),tbl["port"].cast<int>(),tbl["timeout"].cast<double>());
            else if (tbl.isMember("path"))
                remote = new FileCommunication(tbl["path"].cast<string>());
            else{
                throw runtime_error("Remote communication must be ethernet- or file-baed");
            }
            LeCroy6Zi *tmpscope = new LeCroy6Zi(remote);
            tmpscope->stop();
        } catch (runtime_error &e) {
            cout << "Could not stop scope! : " << e.what() << endl;
        }
    }
    if (digitizers.size() > 0) digitizers[arm_last]->stopAcquisition();
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (i == arm_last) continue;
        digitizers[i]->stopAcquisition();
    }
    pthread_cond_signal(&newdata);

    //busy wait for all data to be written out
//  while (decode_running) { sleep(1); }

    pthread_join(decode, NULL);
    pthread_join(software_trigger, NULL);
//  pthread_exit(NULL);

    // clean-up
    delete runtype;
    for (size_t i = 0 ; i < settings.size() ; i++){
        delete settings[i];
    }
    for (size_t i = 0 ; i < buffers.size() ; i++){
        delete buffers[i];
    }
    for (size_t i = 0 ; i < digitizers.size() ; i++){
        delete digitizers[i];
    }
    for (size_t i = 0 ; i < decoders.size() ; i++){
        delete decoders[i];
    }
    map<string, BoardCommManager*>::iterator it;
    for (it = communications.begin() ; it != communications.end() ; it++){
        delete it->second;
    }

    pthread_exit(NULL);
}

#endif
