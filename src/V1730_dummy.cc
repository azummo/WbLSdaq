/**
 *  Copyright 2014 by Benjamin Land (a.k.a. BenLand100)
 *
 *  WbLSdaq is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  WbLSdaq is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with WbLSdaq. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <cmath>
#include <iostream>
#include <stdexcept>
 
#include "V1730_dummy.hh"

using namespace std;

V1730_dummySettings::V1730_dummySettings() : DigitizerSettings("") {
    //These are "do nothing" defaults
    card.dual_trace = 0; // 1 bit
    card.analog_probe = 0; // 2 bit (see docs)
    card.oscilloscope_mode = 1; // 1 bit
    card.digital_virt_probe_1 = 0; // 3 bit (see docs)
    card.digital_virt_probe_2 = 0; // 3 bit (see docs)
    card.coincidence_window = 1; // 3 bit
    card.global_majority_level = 0; // 3 bit
    card.external_global_trigger = 0; // 1 bit
    card.software_global_trigger = 0; // 1 bit
    card.out_logic = 0; // 2 bit (OR,AND,MAJORITY)
    card.out_majority_level = 0; // 3 bit
    card.external_trg_out = 0; // 1 bit
    card.software_trg_out = 0; // 1 bit
    card.max_board_agg_blt = 5;
    
    for (uint32_t ch = 0; ch < 16; ch++) {
        chanDefaults(ch);
    }
    
}

V1730_dummySettings::V1730_dummySettings(RunTable &digitizer, RunDB &db) : DigitizerSettings(digitizer.getIndex()){
    
    card.dual_trace = 0; // 1 bit
    card.analog_probe = 0; // 2 bit (see docs)
    card.oscilloscope_mode = 1; // 1 bit
    card.digital_virt_probe_1 = 0; // 3 bit (see docs)
    card.digital_virt_probe_2 = 0; // 3 bit (see docs)
    
    card.coincidence_window = digitizer["coincidence_window"].cast<int>(); // 3 bit
    card.global_majority_level = digitizer["global_majority_level"].cast<int>(); // 3 bit
    card.external_global_trigger = digitizer["external_trigger_enable"].cast<bool>() ? 1 : 0; // 1 bit
    card.software_global_trigger = digitizer["software_trigger_enable"].cast<bool>() ? 1 : 0; // 1 bit
    card.out_logic = digitizer["trig_out_logic"].cast<int>(); // 2 bit (OR,AND,MAJORITY)
    card.out_majority_level = digitizer["trig_out_majority_level"].cast<int>(); // 3 bit
    card.external_trg_out = digitizer["external_trigger_out"].cast<bool>() ? 1 : 0; // 1 bit
    card.software_trg_out = digitizer["external_trigger_out"].cast<bool>() ? 1 : 0; // 1 bit
    card.max_board_agg_blt = digitizer["aggregates_per_transfer"].cast<int>(); 
    
    for (int ch = 0; ch < 16; ch++) {
        if (ch%2 == 0) {
            string grname = "GR"+to_string(ch/2);
            if (!db.tableExists(grname,index)) {
                groupDefaults(ch/2);
            } else {
                cout << "\t" << grname << endl;
                RunTable group = db.getTable(grname,index);
                
                groups[ch/2].local_logic = group["local_logic"].cast<int>(); // 2 bit (see docs)
                groups[ch/2].valid_logic = group["valid_logic"].cast<int>(); // 2 bit (see docs)
                groups[ch/2].global_trigger = group["request_global_trigger"].cast<bool>() ? 1 : 0; // 1 bit
                groups[ch/2].trg_out = group["request_trig_out"].cast<bool>() ? 1 : 0; // 1 bit
                groups[ch/2].valid_mask = 0;
                vector<bool> mask = group["validation_mask"].toVector<bool>();
                for (size_t i = 0; i < mask.size(); i++) {
                    groups[ch/2].valid_mask |= (mask[i] ? 1 : 0) << i;
                }
                groups[ch/2].valid_mode = group["validation_mode"].cast<int>(); // 2 bit
                groups[ch/2].valid_majority = group["validation_majority_level"].cast<int>(); // 3 bit
                groups[ch/2].record_length = group["record_length"].cast<int>(); // 16* bit
                groups[ch/2].ev_per_buffer = group["events_per_buffer"].cast<int>(); // 10 bit
            
            }
        }
        string chname = "CH"+to_string(ch);
        if (!db.tableExists(chname,index)) {
            chanDefaults(ch);
        } else {
            cout << "\t" << chname << endl;
            RunTable channel = db.getTable(chname,index);
    
            chans[ch].fixed_baseline = 0; // 12 bit
            
            chans[ch].enabled = channel["enabled"].cast<bool>() ? 1 : 0; //1 bit
            chans[ch].dc_offset = round((-channel["dc_offset"].cast<double>()+1.0)/2.0*pow(2.0,16.0)); // 16 bit (-1V to 1V)
            chans[ch].baseline_mean = channel["baseline_average"].cast<int>(); // 3 bit (fixed,16,64,256,1024)
            chans[ch].pulse_polarity = channel["pulse_polarity"].cast<int>(); // 1 bit (0->positive, 1->negative)
            chans[ch].trg_threshold =  channel["trigger_threshold"].cast<int>();// 12 bit
            chans[ch].self_trigger = channel["self_trigger"].cast<bool>() ? 0 : 1; // 1 bit (0->enabled, 1->disabled)
            chans[ch].pre_trigger = channel["pre_trigger"].cast<int>(); // 9* bit
            chans[ch].gate_offset = channel["gate_offset"].cast<int>(); // 8 bit
            chans[ch].long_gate = channel["long_gate"].cast<int>(); // 12 bit
            chans[ch].short_gate = channel["short_gate"].cast<int>(); // 12 bit
            chans[ch].charge_sensitivity = channel["charge_sensitivity"].cast<int>(); // 3 bit (see docs)
            chans[ch].shaped_trigger_width = channel["shaped_trigger_width"].cast<int>(); // 10 bit
            chans[ch].trigger_holdoff = channel["trigger_holdoff"].cast<int>(); // 10* bit
            chans[ch].trigger_config = channel["trigger_type"].cast<int>(); // 2 bit (see docs)
	    chans[ch].dynamic_range = channel["dynamic_range"].cast<int>(); // 1 bit (see docs 0->2Vpp, 1->0.5Vpp)
        }
    }

    // ejc
    this->address = static_cast<uint32_t>(digitizer["address"].cast<int>());
    this->word	  = static_cast<uint32_t>(digitizer["word"].cast<int>());
}

V1730_dummySettings::~V1730_dummySettings() {

}
        
void V1730_dummySettings::validate() { //FIXME validate bit fields too
    for (int ch = 0; ch < 16; ch++) {
        if (ch % 2 == 0) {
            if (groups[ch/2].record_length > 65535) throw runtime_error("Number of samples exceeds 65535 (gr " + to_string(ch/2) + ")");
            if (groups[ch/2].ev_per_buffer < 2) throw runtime_error("Number of events per channel buffer must be at least 2 (gr " + to_string(ch/2) + ")");
            if (groups[ch/2].ev_per_buffer > 1023) throw runtime_error("Number of events per channel buffer exceeds 1023 (gr " + to_string(ch/2) + ")");
        }
        if (chans[ch].pre_trigger > 2044) throw runtime_error("Pre trigger samples exceeds 2044 (ch " + to_string(ch) + ")");
//      if (chans[ch].short_gate > 4095) throw runtime_error("Short gate samples exceeds 4095 (ch " + to_string(ch) + ")");
//      if (chans[ch].long_gate > 4095) throw runtime_error("Long gate samples exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].gate_offset > 255) throw runtime_error("Gate offset samples exceeds 255 (ch " + to_string(ch) + ")");
        if (chans[ch].pre_trigger < chans[ch].gate_offset + 19) throw runtime_error("Gate offset and pre_trigger relationship violated (ch " + to_string(ch) + ")");
        if (chans[ch].trg_threshold > 4095) throw runtime_error("Trigger threshold exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].fixed_baseline > 4095) throw runtime_error("Fixed baseline exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].shaped_trigger_width > 1023) throw runtime_error("Shaped trigger width exceeds 1023 (ch " + to_string(ch) + ")");
        if (chans[ch].trigger_holdoff > 4092) throw runtime_error("Trigger holdoff width exceeds 4092 (ch " + to_string(ch) + ")");
        if (chans[ch].dc_offset > 65535) throw runtime_error("DC Offset cannot exceed 65535 (ch " + to_string(ch) + ")");
    }
}

void V1730_dummySettings::chanDefaults(uint32_t ch) {
    chans[ch].enabled = 0; //1 bit
    chans[ch].dynamic_range = 0; // 1 bit
    chans[ch].pre_trigger = 30; // 9* bit
    chans[ch].trg_threshold = 100; // 12 bit
    chans[ch].fixed_baseline = 0; // 12 bit
    chans[ch].shaped_trigger_width = 10; // 10 bit
    chans[ch].trigger_holdoff = 30; // 10* bit
    chans[ch].charge_sensitivity = 000; // 3 bit (see docs)
    chans[ch].pulse_polarity = 1; // 1 bit (0->positive, 1->negative)
    chans[ch].trigger_config = 0; // 2 bit (see docs)
    chans[ch].baseline_mean = 3; // 3 bit (fixed,16,64,256,1024)
    chans[ch].self_trigger = 1; // 1 bit (0->enabled, 1->disabled)
    chans[ch].dc_offset = 0x8000; // 16 bit (-1V to 1V)
}

void V1730_dummySettings::groupDefaults(uint32_t gr) {
    groups[gr].local_logic = 3; // 2 bit
    groups[gr].valid_logic = 3; // 2 bit
    groups[gr].global_trigger = 0; // 1 bit
    groups[gr].trg_out = 0; // 1 bit
    groups[gr].record_length = 200; // 16* bit
    groups[gr].ev_per_buffer = 50; // 10 bit
}

V1730_dummy::V1730_dummy(VMEBridge &_bridge, uint32_t _baseaddr) : Digitizer(_bridge,_baseaddr) {

}

V1730_dummy::~V1730_dummy() {
    //Fully reset the board just in case
    write32(REG_BOARD_CONFIGURATION_RELOAD,0);
}

void V1730_dummy::calib() {
    write32(REG_CHANNEL_CALIB,0xAAAAAAAA);
}

bool V1730_dummy::program(DigitizerSettings &_settings) {
    V1730_dummySettings &settings = dynamic_cast<V1730_dummySettings&>(_settings);
    try {
        settings.validate();
    } catch (runtime_error &e) {
        cout << "Could not program V1730_dummy: " << e.what() << endl;
        return false;
    }

//  write32(REG_BOARD_CONFIGURATION_RELOAD,0);

    write32(settings.address, settings.word);
    
    //Set max board aggregates to transver per readout
//  write16(REG_READOUT_BLT_AGGREGATE_NUMBER,settings.card.max_board_agg_blt);
    
    //Enable VME BLT readout
//  write16(REG_READOUT_CONTROL,1<<4);
    
    return true;
}

void V1730_dummy::softTrig() {
    write32(REG_SOFTWARE_TRIGGER,0xDEADBEEF);
}

void V1730_dummy::startAcquisition() {
    write32(REG_ACQUISITION_CONTROL,1<<2);
}

void V1730_dummy::stopAcquisition() {
    write32(REG_ACQUISITION_CONTROL,0);
}

bool V1730_dummy::acquisitionRunning() {
    return read32(REG_ACQUISITION_STATUS) & (1 << 2);
}

bool V1730_dummy::readoutReady() {
    return read32(REG_ACQUISITION_STATUS) & (1 << 3);
}


bool V1730_dummy::checkTemps(vector<uint32_t> &temps, uint32_t danger) {
    temps.resize(16);
    bool over = false;
    for (int ch = 0; ch < 16; ch++) {
        temps[ch] = read32(REG_CHANNEL_TEMP|(ch<<8));
        if (temps[ch] >= danger) over = true;
    }
    return over;
}

size_t V1730_dummy::readoutBLT_evtsz(char *buffer, size_t buffer_size) {
    size_t offset = 0, total = 0;
    while (readoutReady()) {
        uint32_t next = read32(REG_EVENT_SIZE);
        total += 4*next;
        if (offset+total > buffer_size) throw runtime_error("Readout buffer too small!");
        size_t lastoff = offset;
        while (offset < total) {
            size_t remaining = total-offset, read;
            if (remaining > 4090) {
                read = readBLT(0x0000, buffer+offset, 4090);
            } else {
                remaining = 8*(remaining%8 ? remaining/8+1 : remaining/8); // needs to be multiples of 8 (64bit)
                read = readBLT(0x0000, buffer+offset, remaining);
            }
            offset += read;
            if (!read) {
                cout << "\tfailed event size " << offset-lastoff << " / " << next*4 << endl;
                break;
            }
        }
    }
    return total;
}



V1730_dummyDecoder::V1730_dummyDecoder(size_t _eventBuffer, V1730_dummySettings &_settings) : eventBuffer(_eventBuffer), settings(_settings) {

    dispatch_index = decode_counter = chanagg_counter = boardagg_counter = 0;
    
    for (size_t ch = 0; ch < 16; ch++) {
        if (settings.getEnabled(ch)) {
            chan2idx[ch] = nsamples.size();
            idx2chan[nsamples.size()] = ch;
            nsamples.push_back(settings.getRecordLength(ch));
            grabbed.push_back(0);
            if (eventBuffer > 0) {
                grabs.push_back(new uint16_t[eventBuffer*nsamples.back()]);
                patterns.push_back(new uint16_t[eventBuffer]);
                baselines.push_back(new uint16_t[eventBuffer]);
                qshorts.push_back(new uint16_t[eventBuffer]);
                qlongs.push_back(new uint16_t[eventBuffer]);
                times.push_back(new uint64_t[eventBuffer]);
            }
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC,&last_decode_time);

}

V1730_dummyDecoder::~V1730_dummyDecoder() {
    for (size_t i = 0; i < grabs.size(); i++) {
        delete [] grabs[i];
        delete [] patterns[i];
        delete [] baselines[i];
        delete [] qshorts[i];
        delete [] qlongs[i];
        delete [] times[i];
    }
}

void V1730_dummyDecoder::decode(Buffer &buf) {
//  vector<size_t> lastgrabbed(grabbed);
//  
//  decode_size = buf.fill();
//  cout << settings.getIndex() << " decoding " << decode_size << " bytes." << endl;
//  uint32_t *next = (uint32_t*)buf.rptr(), *start = (uint32_t*)buf.rptr();
//  while ((size_t)((next = decode_board_agg(next)) - start + 1)*4 < decode_size);
//  buf.dec(decode_size);
//  decode_counter++;
//  
//  struct timespec cur_time;
//  clock_gettime(CLOCK_MONOTONIC,&cur_time);
//  double time_int = (cur_time.tv_sec - last_decode_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_decode_time.tv_nsec);
//  last_decode_time = cur_time;
//  
//  for (size_t i = 0; i < idx2chan.size(); i++) {
//      cout << "\tch" << idx2chan[i] << "\tev: " << grabbed[i]-lastgrabbed[i] << " / " << (grabbed[i]-lastgrabbed[i])/time_int << " Hz / " << grabbed[i] << " total" << endl;
//  }
}

size_t V1730_dummyDecoder::eventsReady() {
    size_t grabs = grabbed[0];
    for (size_t idx = 1; idx < grabbed.size(); idx++) {
        if (grabbed[idx] < grabs) grabs = grabbed[idx];
    }
    return grabs;
}

// length, lvdsidx, dsize, nsamples, samples[], strlen, strname[]

void V1730_dummyDecoder::dispatch(int nfd, int *fds) {
    
    size_t ready = eventsReady();
    
    for ( ; dispatch_index < ready; dispatch_index++) {
        for (size_t i = 0; i < nsamples.size(); i++) {
            uint8_t lvdsidx = patterns[i][dispatch_index] & 0xFF; 
            uint8_t dsize = 2;
            uint16_t nsamps = nsamples[i];
            uint16_t *samples = &grabs[i][nsamps*dispatch_index];
            string strname = "/"+settings.getIndex()+"/ch" + to_string(idx2chan[i]);
            uint16_t strlen = strname.length();
            uint16_t length = 2+strlen+2+nsamps*2+1+1;
            for (int j = 0; j < nfd; j++) {
                writeall(fds[j],&length,2);
                writeall(fds[j],&lvdsidx,1);
                writeall(fds[j],&dsize,1);
                writeall(fds[j],&nsamps,2);
                writeall(fds[j],samples,nsamps*2);
                writeall(fds[j],&strlen,2);
                writeall(fds[j],strname.c_str(),strlen);
            }
        }
    }
}

using namespace H5;

void V1730_dummyDecoder::writeOut(H5File &file, size_t nEvents) {

    cout << "\t/" << settings.getIndex() << endl;

    Group cardgroup = file.createGroup("/"+settings.getIndex());
        
    DataSpace scalar(0,NULL);
    
    double dval;
    uint32_t ival;
    
    Attribute bits = cardgroup.createAttribute("bits",PredType::NATIVE_UINT32,scalar);
    ival = 14;
    bits.write(PredType::NATIVE_INT32,&ival);
    
    Attribute ns_sample = cardgroup.createAttribute("ns_sample",PredType::NATIVE_DOUBLE,scalar);
    dval = 2.0;
    ns_sample.write(PredType::NATIVE_DOUBLE,&dval);
    
    for (size_t i = 0; i < nsamples.size(); i++) {
    
        string chname = "ch" + to_string(idx2chan[i]);
        Group group = cardgroup.createGroup(chname);
        string groupname = "/"+settings.getIndex()+"/"+chname;
        
        cout << "\t" << groupname << endl;
        
        Attribute offset = group.createAttribute("offset",PredType::NATIVE_UINT32,scalar);
        ival = settings.getDCOffset(idx2chan[i]);
        offset.write(PredType::NATIVE_UINT32,&ival);
        
        Attribute samples = group.createAttribute("samples",PredType::NATIVE_UINT32,scalar);
        ival = settings.getRecordLength(idx2chan[i]);
        samples.write(PredType::NATIVE_UINT32,&ival);
        
        Attribute presamples = group.createAttribute("presamples",PredType::NATIVE_UINT32,scalar);
        ival = settings.getPreSamples(idx2chan[i]);
        presamples.write(PredType::NATIVE_UINT32,&ival);
        
        Attribute threshold = group.createAttribute("threshold",PredType::NATIVE_UINT32,scalar);
        ival = settings.getThreshold(idx2chan[i]);
        threshold.write(PredType::NATIVE_UINT32,&ival);
        
        hsize_t dimensions[2];
        dimensions[0] = nEvents;
        dimensions[1] = nsamples[i];
        
        DataSpace samplespace(2, dimensions);
        DataSpace metaspace(1, dimensions);
        
        cout << "\t" << groupname << "/samples" << endl;
        DataSet samples_ds = file.createDataSet(groupname+"/samples", PredType::NATIVE_UINT16, samplespace);
        samples_ds.write(grabs[i], PredType::NATIVE_UINT16);
        memmove(grabs[i],grabs[i]+nEvents*nsamples[i],nsamples[i]*sizeof(uint16_t)*(grabbed[i]-nEvents));
        
        cout << "\t" << groupname << "/patterns" << endl;
        DataSet patterns_ds = file.createDataSet(groupname+"/patterns", PredType::NATIVE_UINT16, metaspace);
        patterns_ds.write(patterns[i], PredType::NATIVE_UINT16);
        memmove(patterns[i],patterns[i]+nEvents,sizeof(uint16_t)*(grabbed[i]-nEvents));
        
        cout << "\t" << groupname << "/baselines" << endl;
        DataSet baselines_ds = file.createDataSet(groupname+"/baselines", PredType::NATIVE_UINT16, metaspace);
        baselines_ds.write(baselines[i], PredType::NATIVE_UINT16);
        memmove(baselines[i],baselines[i]+nEvents,sizeof(uint16_t)*(grabbed[i]-nEvents));
        
        cout << "\t" << groupname << "/qshorts" << endl;
        DataSet qshorts_ds = file.createDataSet(groupname+"/qshorts", PredType::NATIVE_UINT16, metaspace);
        qshorts_ds.write(qshorts[i], PredType::NATIVE_UINT16);
        memmove(qshorts[i],qshorts[i]+nEvents,sizeof(uint16_t)*(grabbed[i]-nEvents));
        
        cout << "\t" << groupname << "/qlongs" << endl;
        DataSet qlongs_ds = file.createDataSet(groupname+"/qlongs", PredType::NATIVE_UINT16, metaspace);
        qlongs_ds.write(qlongs[i], PredType::NATIVE_UINT16);
        memmove(qlongs[i],qlongs[i]+nEvents,sizeof(uint16_t)*(grabbed[i]-nEvents));

        cout << "\t" << groupname << "/times" << endl;
        DataSet times_ds = file.createDataSet(groupname+"/times", PredType::NATIVE_UINT64, metaspace);
        times_ds.write(times[i], PredType::NATIVE_UINT64);
        memmove(times[i],times[i]+nEvents,sizeof(uint64_t)*(grabbed[i]-nEvents));
        
        grabbed[i] -= nEvents;
    }
    
    dispatch_index -= nEvents;
    if (dispatch_index < 0) dispatch_index = 0;
}

uint32_t* V1730_dummyDecoder::decode_chan_agg(uint32_t *chanagg, uint32_t chn, uint16_t pattern) {

    const bool format_flag = chanagg[0] & 0x80000000;
    if (!format_flag) throw runtime_error("Channel format not found");
    
    chanagg_counter++;
   
    //const uint32_t size = chanagg[0] & 0x7FFF;
    //const uint32_t format = chanagg[1];
    //const uint32_t samples = (format & 0xFFF)*8;
   
    
    const uint32_t idx = chan2idx[chn];
    const uint32_t len = nsamples[idx];

    if (idx == 999) throw runtime_error("Received data for disabled channel (" + to_string(chn) + ")");

    //if (len != samples) throw runtime_error("Number of samples received " + to_string(samples) + " does not match expected " + to_string(len) + " (" + to_string(idx2chan[idx]) + ")");
        
    if (eventBuffer) {
        const size_t ev = grabbed[idx]++;
        if (ev == eventBuffer) throw runtime_error("Decoder buffer for " + settings.getIndex() + " overflowed!");
        uint16_t *data = grabs[idx] + ev*len;
        //for (uint32_t *word = event+1, sample = 0; sample < len; word++, sample+=2) {
	for (uint32_t *word = chanagg, sample = 0; sample < len; word++, sample+=2) {
      	        data[sample+0] = *word & 0x3FFF;
                data[sample+1] = (*word >> 16) & 0x3FFF;
        }
            
        patterns[idx][ev] = pattern;
    } else {
        grabbed[idx]++;
    }
    
    //return chanagg + size;
    return chanagg + len/2;

}

uint32_t* V1730_dummyDecoder::decode_board_agg(uint32_t *boardagg) {
    if (boardagg[0] == 0xFFFFFFFF) {
        boardagg++; //sometimes padded
    }
    if ((boardagg[0] & 0xF0000000) != 0xA0000000) 
        throw runtime_error("Board aggregate missing tag");
    
    boardagg_counter++;    
    
    uint32_t size = boardagg[0] & 0x0FFFFFFF;
    
    //const uint32_t board = (boardagg[1] >> 28) & 0xF;
    //const bool fail = boardagg[1] & (1 << 26);
    //const uint16_t pattern = (boardagg[1] >> 8) & 0x7FFF;
    const uint16_t pattern = (boardagg[1] >> 8) & 0xFFFF; // this has been changed
    const uint16_t mask = (boardagg[1] & 0xFF) | ((boardagg[2] & 0xFF000000) >> 16);
    //const uint32_t mask = boardagg[1] & 0xFF; // question about the mask -> here we only take 8 masks but in the document, there are 16, we are only taking half of this. Ed's comment: I agree with you that this looks like a bug, but I actually doubt it is. we can do an easy test to determine whether it is or not. for now I would recommend not messing with it at that level, and only change the decoding in decode_chan_aggs to match the channel event structure.

    cout << "\t(LVDS & 0xFF): " << (pattern & 0xFF) << endl;
    
    //const uint32_t count = boardagg[2] & 0x7FFFFF;
    //const uint32_t timetag = boardagg[3];
    
    uint32_t *chans = boardagg+4;
    
    for (uint32_t ch = 0; ch < 16; ch++) { // note again, this only loops through 8 groups with 8 masks. We actually have 16 masks in the event structure.
        if (mask & (1 << ch)) {
            chans = decode_chan_agg(chans,ch,pattern);
        }
    } 
    
    return boardagg+size;
}

