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

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include "V1730.hh"

#include <CAENDigitizer.h>

using namespace std;

// FIXME this needs to be checked for consistency with std firmware
V1730Settings::V1730Settings() : DigitizerSettings("") {
    //These are "do nothing" defaults
    card.trigger_overlap = 0; // 1 bit
    card.test_pattern = 0; // 1 bit
    card.trigger_polarity = 1; // 1 bit
    card.coincidence_window = 1; // 3 bit
    card.global_majority_level = 0; // 3 bit
    card.external_global_trigger = 0; // 1 bit
    card.software_global_trigger = 0; // 1 bit
    card.out_logic = 0; // 2 bit (OR,AND,MAJORITY)
    card.out_majority_level = 0; // 3 bit
    card.external_trg_out = 0; // 1 bit
    card.software_trg_out = 0; // 1 bit
    card.max_board_evt_blt = 5;
    card.use_ettt = 0; // 1 bit

    for (uint32_t ch = 0; ch < 16; ch++) {
        chanDefaults(ch);
    }

}

// FIXME this needs to be checked for consistency with std firmware
V1730Settings::V1730Settings(RunTable &digitizer, RunDB &db) : DigitizerSettings(digitizer.getIndex()){
    // board config
    card.trigger_overlap = digitizer["trigger_overlap"].cast<int>(); // 1 bit
    card.test_pattern = digitizer["test_pattern"].cast<int>(); // 1 bit
    card.trigger_polarity = digitizer["trigger_polarity"].cast<int>(); // 1 bit

    // record length, expressed as custom size on-board
    // FIXME should be rounded up to the next multiple of 10
    card.record_length = digitizer["record_length"].cast<int>(); // # of samples
    card.custom_size = card.record_length / 10; // see docs

    card.post_trigger = digitizer["post_trigger"].cast<int>(); // int

    const uint32_t total_locations = 640000;
    card.buff_org = 0xA;
    // TODO potential bug here, in that 0xA corresponds to 625 != 640 samples
    while (total_locations/(1 << card.buff_org) < card.record_length)
        card.buff_org--;
    if (card.buff_org > 0xA) card.buff_org = 0xA;
    if (card.buff_org < 0x2) card.buff_org = 0x2;
//  cout << "Largest buffer: " << largest_buffer << " loc\nDesired buffers: " << num_buffers << "\nProgrammed buffers: " << (1<<settings.card.buff_org) << endl;

    card.coincidence_window = digitizer["coincidence_window"].cast<int>(); // 3 bit
    card.global_majority_level = digitizer["global_majority_level"].cast<int>(); // 3 bit
    card.external_global_trigger = digitizer["external_trigger_enable"].cast<bool>() ? 1 : 0; // 1 bit
    card.software_global_trigger = digitizer["software_trigger_enable"].cast<bool>() ? 1 : 0; // 1 bit
    card.out_logic = digitizer["trig_out_logic"].cast<int>(); // 2 bit (OR,AND,MAJORITY)
    card.out_majority_level = digitizer["trig_out_majority_level"].cast<int>(); // 3 bit
    card.external_trg_out = digitizer["external_trigger_out"].cast<bool>() ? 1 : 0; // 1 bit
    card.software_trg_out = digitizer["external_trigger_out"].cast<bool>() ? 1 : 0; // 1 bit
    card.max_board_evt_blt = digitizer["events_per_transfer"].cast<int>();

// ATB add ETTT option
    card.use_ettt = digitizer["use_ettt"].cast<bool>() ? 1 : 0; // 1 bit

    for (int ch = 0; ch < 16; ch++) {
        if (ch%2 == 0) {
            string grname = "GR"+to_string(ch/2);
            if (!db.tableExists(grname,index)) {
                groupDefaults(ch/2);
            } else {
                cout << "\t" << grname << endl;
                RunTable group = db.getTable(grname,index);

                groups[ch/2].local_logic = group["local_logic"].cast<int>(); // 2 bit (see docs)
		groups[ch/2].request_duration = group["request_duration"].cast<int>(); // 1 bit
                groups[ch/2].global_trigger = group["request_global_trigger"].cast<bool>() ? 1 : 0; // 1 bit
                groups[ch/2].trg_out = group["request_trig_out"].cast<bool>() ? 1 : 0; // 1 bit

            }
        }
        string chname = "CH"+to_string(ch);
        if (!db.tableExists(chname,index)) {
            chanDefaults(ch);
        } else {
            cout << "\t" << chname << endl;
            RunTable channel = db.getTable(chname,index);

            chans[ch].enabled = channel["enabled"].cast<bool>() ? 1 : 0; //1 bit
            // FIXME this needs to be tested
//          chans[ch].dc_offset = round((-channel["dc_offset"].cast<double>()+1.0)/2.0*pow(2.0,16.0)); // 16 bit (-1V to 1V)
//          chans[ch].dc_offset = channel["dc_offset"].cast<double>() * (pow(2, 16) - 1);
            chans[ch].dc_offset = channel["dc_offset"].cast<double>();
            chans[ch].trg_threshold =  channel["trigger_threshold"].cast<int>();// 12 bit
            chans[ch].shaped_trigger_width = channel["shaped_trigger_width"].cast<int>(); // 10 bit
                chans[ch].dynamic_range = channel["dynamic_range"].cast<int>(); // 1 bit (see docs 0->2Vpp, 1->0.5Vpp)
        }
    }
}

V1730Settings::~V1730Settings() {

}

// FIXME need complete validation for implemented configuration
void V1730Settings::validate() { //FIXME validate bit fields too
    if ((card.record_length < 1) || (640000 < card.record_length)){
        throw runtime_error("Record length must be between 1 and 640000 samples");
    }
    for (int ch = 0; ch < 16; ch++) {
//      FIXME adapt for post_trigger, which is global
//      if (chans[ch].pre_trigger > 2044) throw runtime_error("Pre trigger samples exceeds 2044 (ch " + to_string(ch) + ")");
//      if (chans[ch].trg_threshold > 4095) throw runtime_error("Trigger threshold exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].shaped_trigger_width > 1023) throw runtime_error("Shaped trigger width exceeds 1023 (ch " + to_string(ch) + ")");
        if (chans[ch].dc_offset > 65535) throw runtime_error("DC Offset cannot exceed 65535 (ch " + to_string(ch) + ")");
    }
}

void V1730Settings::chanDefaults(uint32_t ch) {
    chans[ch].enabled = 0; //1 bit
    chans[ch].dynamic_range = 0; // 1 bit
    chans[ch].trg_threshold = 100; // 12 bit
    chans[ch].shaped_trigger_width = 10; // 10 bit
    chans[ch].dc_offset = 0x8000; // 16 bit (-1V to 1V)
}

void V1730Settings::groupDefaults(uint32_t gr) {
    groups[gr].local_logic = 3; // 2 bit
    groups[gr].request_duration = 0; // 1 bit
    groups[gr].global_trigger = 0; // 1 bit
    groups[gr].trg_out = 0; // 1 bit
}

V1730::V1730(BoardCommManager &_bridge, uint32_t _baseaddr) : Digitizer(_bridge,_baseaddr) {
    readableRegisters = {0x1024, 0x1124, 0x1224, 0x1324, 0x1424, 0x1524, 0x1624, 0x1724, 0x1824, 0x1924, 0x1A24, 0x1B24, 0x1C24, 0x1D24, 0x1E24, 0x1F24, 0x1028, 0x1128, 0x1228, 0x1328, 0x1428, 0x1528, 0x1628, 0x1728, 0x1828, 0x1928, 0x1A28, 0x1B28, 0x1C28, 0x1D28, 0x1E28, 0x1F28, 0x1070, 0x1170, 0x1270, 0x1370, 0x1470, 0x1570, 0x1670, 0x1770, 0x1870, 0x1970, 0x1A70, 0x1B70, 0x1C70, 0x1D70, 0x1E70, 0x1F70, 0x1080, 0x1180, 0x1280, 0x1380, 0x1480, 0x1580, 0x1680, 0x1780, 0x1880, 0x1980, 0x1A80, 0x1B80, 0x1C80, 0x1D80, 0x1E80, 0x1F80, 0x1084, 0x1184, 0x1284, 0x1384, 0x1484, 0x1584, 0x1684, 0x1784, 0x1088, 0x1188, 0x1288, 0x1388, 0x1488, 0x1588, 0x1688, 0x1788, 0x1888, 0x1988, 0x1A88, 0x1B88, 0x1C88, 0x1D88, 0x1E88, 0x1F88, 0x108C, 0x118C, 0x128C, 0x138C, 0x148C, 0x158C, 0x168C, 0x178C, 0x188C, 0x198C, 0x1A8C, 0x1B8C, 0x1C8C, 0x1D8C, 0x1E8C, 0x1F8C, 0x1098, 0x1198, 0x1298, 0x1398, 0x1498, 0x1598, 0x1698, 0x1798, 0x1898, 0x1998, 0x1A98, 0x1B98, 0x1C98, 0x1D98, 0x1E98, 0x1F98, 0x10A8, 0x11A8, 0x12A8, 0x13A8, 0x14A8, 0x15A8, 0x16A8, 0x17A8, 0x18A8, 0x19A8, 0x1AA8, 0x1BA8, 0x1CA8, 0x1DA8, 0x1EA8, 0x1FA8, 0x10EC, 0x11EC, 0x12EC, 0x13EC, 0x14EC, 0x15EC, 0x16EC, 0x17EC, 0x18EC, 0x19EC, 0x1AEC, 0x1BEC, 0x1CEC, 0x1DEC, 0x1EEC, 0x1FEC, 0x8000, 0x800C, 0x8020, 0x8100, 0x8104, 0x810C, 0x8110, 0x8114, 0x8118, 0x811C, 0x8120, 0x8124, 0x812C, 0x8138, 0x8140, 0x8144, 0x814C, 0x8168, 0x816C, 0x8170, 0x8178, 0x81A0, 0x81B4, 0x81C4, 0xEF00, 0xEF04, 0xEF08, 0xEF0C, 0xEF10, 0xEF14, 0xEF18, 0xEF1C, 0xEF20, 0xF000, 0xF004, 0xF008, 0xF00C};
}

V1730::~V1730() {
    //Fully reset the board just in case
//  write32(REG_BOARD_CONFIGURATION_RELOAD,0);
    write32(REG_SOFTWARE_RESET,0);
}

void V1730::calib() {
    write32(REG_CHANNEL_CALIB,0xAAAAAAAA);
}

bool V1730::program(DigitizerSettings &_settings) {
    V1730Settings &settings = dynamic_cast<V1730Settings&>(_settings);
    try {
        settings.validate();
    } catch (runtime_error &e) {
        cout << "Could not program V1730: " << e.what() << endl;
        return false;
    }

    //used to build bit fields
    uint32_t data;

    //Fully reset the board just in case
//  write32(REG_BOARD_CONFIGURATION_RELOAD,0);
    write32(REG_SOFTWARE_RESET,0);

    //Front panel config
    data = (1<<0) //ttl
         | (0<<2) | (0<<3) | (0<<4) | (0<<5) //LVDS all input
         | (2<<6) // pattern mode
         | (0<<8) // old lvds features
         | (0<<9);// latch on internal trigger
    data |= 0x00040000; // FIXME hack to propagate CLKOUT to TRGOUT
                        // 0x00040000 == TRGOUT
                        // 0x00050000 == CLKOUT    fires at 125  MHz
                        // 0x00090000 == CLK phase fires at 62.5 MHz
    data |= (settings.card.use_ettt << 22); // ETTT or LVDS pattern
    write32(REG_FRONT_PANEL_CONTROL,data);

    //LVDS new features config
    data = (0<<0) | (0<<4) | (0<<8) | (0<<12); // ignored for now
    write32(REG_LVDS_NEW_FEATURES,data);

    // board configuration
    data = (0 << 0)                               // reserved, must be 0
         | (settings.card.trigger_overlap << 1)   // trigger overlap dis/allowed
         | (0 << 2)                               // reserved, must be 0
         | (settings.card.test_pattern << 3)      // triangular test wave
         | (1 << 4)                               // reserved, must be 1
         | (0 << 5)                               // reserved, must be 0
         | (settings.card.trigger_polarity << 6); // self-trigger polarity
    write32(REG_CONFIG,data);

    //build masks while configuring channels
    uint32_t channel_enable_mask = 0;
    uint32_t global_trigger_mask = (settings.card.coincidence_window << 20)
                                 | (settings.card.global_majority_level << 24)
                                 | (settings.card.external_global_trigger << 30)
                                 | (settings.card.software_global_trigger << 31);
    uint32_t trigger_out_mask = (settings.card.out_logic << 8)
                              | (settings.card.out_majority_level << 10)
                              | (settings.card.external_trg_out << 30)
                              | (settings.card.software_trg_out << 31);

    for (int ch = 0; ch < 16; ch++) {
        channel_enable_mask |= (settings.chans[ch].enabled << ch);

        if (ch % 2 == 0) {
            global_trigger_mask |= (settings.groups[ch/2].global_trigger << (ch/2));
            trigger_out_mask |= (settings.groups[ch/2].trg_out << (ch/2));
        } else {
            /* */
        }

        // FIXME check word structure
        write32(REG_TRIGGER_THRESHOLD|(ch<<8),settings.chans[ch].trg_threshold);
        // FIXME check word structure
        write32(REG_SHAPED_TRIGGER_WIDTH|(ch<<8),settings.chans[ch].shaped_trigger_width);
//      FIXME as far as I can tell, this doesn't exist --- but it should?
//      write32(REG_TRIGGER_HOLDOFF|(ch<<8),settings.chans[ch].trigger_holdoff/4);

        data = (settings.groups[ch/2].local_logic<<0) 	  // interchannel logic
             | (settings.groups[ch/2].request_duration<<2); // pulse duration
        write32(REG_TRIGGER_CTRL|(ch<<8),data);

        write32(REG_DYNAMIC_RANGE|(ch<<8), settings.chans[ch].dynamic_range);
        write32(REG_DC_OFFSET|(ch<<8),settings.chans[ch].dc_offset);
    }

    // FIXME check word structure
    write32(REG_CHANNEL_ENABLE_MASK,channel_enable_mask);
    // FIXME check word structure
    write32(REG_GLOBAL_TRIGGER_MASK,global_trigger_mask);
    // FIXME check word structure
    write32(REG_TRIGGER_OUT_MASK,trigger_out_mask);

    // buffer organization
    write32(REG_BUFF_ORG, settings.card.buff_org);
    write32(REG_CUSTOM_SIZE, settings.card.custom_size);

    // # of samplse to wait before freezing buffer
    write32(REG_POST_TRG, settings.card.post_trigger);

    // force analog monitor to output trigger sum
    write32(REG_MONITOR_MODE, 0);

    //Set max board events to transfer per readout
    write16(REG_READOUT_BLT_EVENT_NUMBER,settings.card.max_board_evt_blt);

    // FIXME this is a temporary hack and should be removed
    // once the semantics of the DC offset are settled
//  write32(0x1098, 0xE665);

    //Enable VME BLT readout
    write16(REG_READOUT_CONTROL,1<<4);

//  readAllRegisters();

    return true;
}

void V1730::softTrig() {
    write32(REG_SOFTWARE_TRIGGER,0xDEADBEEF);
}

void V1730::startAcquisition() {
    write32(REG_ACQUISITION_CONTROL,1<<2);
}

void V1730::stopAcquisition() {
    write32(REG_ACQUISITION_CONTROL,0);
}

bool V1730::acquisitionRunning() {
    return read32(REG_ACQUISITION_STATUS) & (1 << 2);
}

bool V1730::readoutReady() {
    return read32(REG_ACQUISITION_STATUS) & (1 << 3);
}


bool V1730::checkTemps(vector<uint32_t> &temps, uint32_t danger) {
    temps.resize(16);
    bool over = false;
    for (int ch = 0; ch < 16; ch++) {
        temps[ch] = read32(REG_CHANNEL_TEMP|(ch<<8));
        if (temps[ch] >= danger) over = true;
    }
    return over;
}

void V1730::readAllRegisters() {
    uint32_t buffer;
    ios_base::fmtflags flags = cout.flags();
    for (size_t i = 0 ; i < readableRegisters.size() ; i++){
        uint32_t addr = readableRegisters[i];
        buffer = read32(addr);
        cout << hex;
        cout << "read32(0x" << addr << ") = 0x" << buffer << endl;
        cout << dec;
    }
    cout.flags(flags);
}

size_t V1730::readoutBLT_evtsz(char *buffer, size_t buffer_size) {
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



V1730Decoder::V1730Decoder(size_t _eventBuffer, V1730Settings &_settings) : eventBuffer(_eventBuffer), settings(_settings) {

    dispatch_index = decode_counter = chanagg_counter = boardagg_counter = 0;

    counters = vector<uint32_t>();
    timetags = vector<uint32_t>();
    exttimetags = vector<uint16_t>();
    for (size_t ch = 0; ch < 16; ch++) {
        if (settings.getEnabled(ch)) {
            chan2idx[ch] = nsamples.size();
            idx2chan[nsamples.size()] = ch;
            nsamples.push_back(settings.getRecordLength());
            grabbed.push_back(0);
            if (eventBuffer > 0) {
                grabs.push_back(new uint16_t[eventBuffer*nsamples.back()]);
                patterns.push_back(new uint16_t[eventBuffer]);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC,&last_decode_time);

}

V1730Decoder::~V1730Decoder() {
    for (size_t i = 0; i < grabs.size(); i++) {
        delete [] grabs[i];
        delete [] patterns[i];
    }
}

void V1730Decoder::decode(Buffer &buf) {
    vector<size_t> lastgrabbed(grabbed);

    decode_size = buf.fill();
    //cout << settings.getIndex() << " decoding " << decode_size << " bytes." << endl;
    uint32_t *next = (uint32_t*)buf.rptr(), *start = (uint32_t*)buf.rptr();
    while ((size_t)((next = decode_board_agg(next)) - start + 1)*4 < decode_size);
    buf.dec(decode_size);
    decode_counter++;

    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC,&cur_time);
    //double time_int = (cur_time.tv_sec - last_decode_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_decode_time.tv_nsec);
    //last_decode_time = cur_time;

    // double nmb = decode_size / pow(1024, 2);
    // double drate = nmb / time_int;
    // cout << "\t\t data rate: "
    //     << drate << " MB/s"
    //     << endl;
}

size_t V1730Decoder::eventsReady() {
    size_t grabs = grabbed[0];
    for (size_t idx = 1; idx < grabbed.size(); idx++) {
        if (grabbed[idx] < grabs) grabs = grabbed[idx];
    }
    return grabs;
}

using namespace H5;


void V1730Decoder::writeOut(H5File &file, size_t nEvents) {

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

    Attribute samples = cardgroup.createAttribute("samples",PredType::NATIVE_UINT32,scalar);
    ival = static_cast<uint32_t>(settings.getRecordLength());
    samples.write(PredType::NATIVE_UINT32,&ival);

    hsize_t dims[1];
    dims[0] = nEvents;
    DataSpace space(1, dims);

    DataSet counters_ds = cardgroup.createDataSet("counters", PredType::NATIVE_UINT32, space);
    counters_ds.write(counters.data(), PredType::NATIVE_UINT32);
    DataSet timetags_ds = cardgroup.createDataSet("timetags", PredType::NATIVE_UINT32, space);
    timetags_ds.write(timetags.data(), PredType::NATIVE_UINT32);
    DataSet exttimetags_ds = cardgroup.createDataSet("exttimetags", PredType::NATIVE_UINT16, space);
    exttimetags_ds.write(exttimetags.data(), PredType::NATIVE_UINT16);

    std::cout << "Event count: " << counters.size() << std::endl; 

    for (size_t i = 0; i < nsamples.size(); i++) {

        string chname = "ch" + to_string(idx2chan[i]);
        Group group = cardgroup.createGroup(chname);
        string groupname = "/"+settings.getIndex()+"/"+chname;

        Attribute offset = group.createAttribute("offset",PredType::NATIVE_UINT32,scalar);
        ival = settings.getDCOffset(idx2chan[i]);
        offset.write(PredType::NATIVE_UINT32,&ival);

        Attribute threshold = group.createAttribute("threshold",PredType::NATIVE_UINT32,scalar);
        ival = settings.getThreshold(idx2chan[i]);
        threshold.write(PredType::NATIVE_UINT32,&ival);

	Attribute dynamic_range = group.createAttribute("dynamic_range",PredType::NATIVE_DOUBLE,scalar);
	dval = settings.getDynamicRange(idx2chan[i]);
	dynamic_range.write(PredType::NATIVE_DOUBLE,&dval);

        hsize_t dimensions[2];
        dimensions[0] = nEvents;
        dimensions[1] = nsamples[i];

        DataSpace samplespace(2, dimensions);
        DataSpace metaspace(1, dimensions);

        DataSet samples_ds = file.createDataSet(groupname+"/samples", PredType::NATIVE_UINT16, samplespace);
        samples_ds.write(grabs[i], PredType::NATIVE_UINT16);
        memmove(grabs[i],grabs[i]+nEvents*nsamples[i],nsamples[i]*sizeof(uint16_t)*(grabbed[i]-nEvents));

        DataSet patterns_ds = file.createDataSet(groupname+"/patterns", PredType::NATIVE_UINT16, metaspace);
        patterns_ds.write(patterns[i], PredType::NATIVE_UINT16);
        memmove(patterns[i],patterns[i]+nEvents,sizeof(uint16_t)*(grabbed[i]-nEvents));

//      TODO might be worth monitoring somehow
//      cout << "\t" << groupname << "/baselines" << endl;
//      DataSet baselines_ds = file.createDataSet(groupname+"/baselines", PredType::NATIVE_UINT16, metaspace);
//      baselines_ds.write(baselines[i], PredType::NATIVE_UINT16);
//      memmove(baselines[i],baselines[i]+nEvents,sizeof(uint16_t)*(grabbed[i]-nEvents));

        grabbed[i] -= nEvents;
    }

    timetags.clear();
    exttimetags.clear();
    counters.clear();

    dispatch_index -= nEvents;
    if (dispatch_index < 0) dispatch_index = 0;
}

uint32_t* V1730Decoder::decode_chan_agg(uint32_t *chanagg, uint32_t chn, uint16_t pattern) {

    // TODO a quality check for every 32-bit sample word would be:
    // assert((word & 0xC000C000) | (0xC000C000) == 0)
    // which confirms that bits 30:31 and 14:15 are 0, in accordance with docs
//  const bool format_flag = ...
//  if (!format_flag) throw runtime_error("Channel format not found");

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
                uint32_t content = *word;
                bool condition = ((content & 0xC000C000) == 0);
                if (!condition){
                    throw runtime_error("sample word & 0xC000C000 != 0");
                }
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

uint32_t* V1730Decoder::decode_board_agg(uint32_t *boardagg) {
    if (boardagg[0] == 0xFFFFFFFF) {
        boardagg++; //sometimes padded
    }
    if ((boardagg[0] & 0xF0000000) != 0xA0000000){
        throw runtime_error("Board aggregate missing tag");
    }
    boardagg_counter++;

    uint32_t size = boardagg[0] & 0x0FFFFFFF;

    //const uint32_t board = (boardagg[1] >> 28) & 0xF;
    const bool fail = boardagg[1] & (1 << 26);
    if (fail){
        throw runtime_error("board fail flag set " + to_string(boardagg_counter));
    }
    //const uint16_t pattern = (boardagg[1] >> 8) & 0x7FFF;
    const uint16_t pattern = settings.getETTTEnabled() ? 0 : ((boardagg[1] >> 8) & 0xFFFF); // this has been changed; 0 if ETTT mode, else LVDS I/O pattern
    const uint16_t mask = (boardagg[1] & 0xFF) | ((boardagg[2] & 0xFF000000) >> 16);
    //const uint32_t mask = boardagg[1] & 0xFF; // question about the mask -> here we only take 8 masks but in the document, there are 16, we are only taking half of this. Ed's comment: I agree with you that this looks like a bug, but I actually doubt it is. we can do an easy test to determine whether it is or not. for now I would recommend not messing with it at that level, and only change the decoding in decode_chan_aggs to match the channel event structure.

    const uint32_t counter = static_cast<uint32_t>(boardagg[2] & 0x00FFFFFF);
    const uint32_t timetag = boardagg[3];
    const uint16_t exttimetag = settings.getETTTEnabled() ? ((boardagg[1] >> 8) & 0xFFFF) : 0 ; // ETTT bits if ETTT mode, else 0
    counters.push_back(counter);
    timetags.push_back(timetag);
    exttimetags.push_back(exttimetag);

    uint32_t *chans = boardagg+4;

    for (uint32_t ch = 0; ch < 16; ch++) { // note again, this only loops through 8 groups with 8 masks. We actually have 16 masks in the event structure.
        if (mask & (1 << ch)) {
            chans = decode_chan_agg(chans,ch,pattern);
        }
    }
    if (chans != boardagg+size){
        throw runtime_error("predicted end of board aggregate != end of channel aggregates");
    }

    return boardagg+size;
}

