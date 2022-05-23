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
 
#include "V1730_wavedump.hh"

using namespace std;

// FIXME this needs to be checked for consistency with std firmware
V1730_wavedumpSettings::V1730_wavedumpSettings() : DigitizerSettings("") {
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
    
    for (uint32_t ch = 0; ch < 16; ch++) {
        chanDefaults(ch);
    }
    
}

// FIXME this needs to be checked for consistency with std firmware
V1730_wavedumpSettings::V1730_wavedumpSettings(RunTable &digitizer, RunDB &db) : DigitizerSettings(digitizer.getIndex()){
    // board config
    card.trigger_overlap = digitizer["trigger_overlap"].cast<int>(); // 1 bit
    card.test_pattern = digitizer["test_pattern"].cast<int>(); // 1 bit
    card.trigger_polarity = digitizer["trigger_polarity"].cast<int>(); // 1 bit

    // record length, expressed as custom size on-board
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

    for (int ch = 0; ch < 16; ch++) {
        if (ch%2 == 0) {
            string grname = "GR"+to_string(ch/2);
            if (!db.tableExists(grname,index)) {
                groupDefaults(ch/2);
            } else {
                cout << "\t" << grname << endl;
                RunTable group = db.getTable(grname,index);
                
                groups[ch/2].local_logic = group["local_logic"].cast<int>(); // 2 bit (see docs)
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
            chans[ch].dc_offset = 0.0;
            chans[ch].trg_threshold =  channel["trigger_threshold"].cast<int>();// 12 bit
            chans[ch].shaped_trigger_width = channel["shaped_trigger_width"].cast<int>(); // 10 bit
	        chans[ch].dynamic_range = channel["dynamic_range"].cast<int>(); // 1 bit (see docs 0->2Vpp, 1->0.5Vpp)
        }
    }
}

V1730_wavedumpSettings::~V1730_wavedumpSettings() {

}
        
// FIXME need complete validation for implemented configuration
void V1730_wavedumpSettings::validate() { //FIXME validate bit fields too
    if ((card.record_length < 1) || (640000 < card.record_length)){
        throw runtime_error("Record length must be between 1 and 640000 samples");
    }
    for (int ch = 0; ch < 16; ch++) {
//      FIXME adapt for post_trigger, which is global
//      if (chans[ch].pre_trigger > 2044) throw runtime_error("Pre trigger samples exceeds 2044 (ch " + to_string(ch) + ")");
        if (chans[ch].trg_threshold > 4095) throw runtime_error("Trigger threshold exceeds 4095 (ch " + to_string(ch) + ")");
        if (chans[ch].shaped_trigger_width > 1023) throw runtime_error("Shaped trigger width exceeds 1023 (ch " + to_string(ch) + ")");
        if (chans[ch].dc_offset > 65535) throw runtime_error("DC Offset cannot exceed 65535 (ch " + to_string(ch) + ")");
    }
}

void V1730_wavedumpSettings::chanDefaults(uint32_t ch) {
    chans[ch].enabled = 0; //1 bit
    chans[ch].dynamic_range = 0; // 1 bit
    chans[ch].trg_threshold = 100; // 12 bit
    chans[ch].shaped_trigger_width = 10; // 10 bit
    chans[ch].dc_offset = 0x8000; // 16 bit (-1V to 1V)
}

void V1730_wavedumpSettings::groupDefaults(uint32_t gr) {
    groups[gr].local_logic = 3; // 2 bit
    groups[gr].global_trigger = 0; // 1 bit
    groups[gr].trg_out = 0; // 1 bit
}

V1730_wavedump::V1730_wavedump(VMEBridge &_bridge, uint32_t _baseaddr) : Digitizer(_bridge,_baseaddr) {
    /* */
}

V1730_wavedump::~V1730_wavedump() {
    //Fully reset the board just in case
    write32(REG_BOARD_CONFIGURATION_RELOAD,0);
}

void V1730_wavedump::calib() {
    write32(REG_CHANNEL_CALIB,0xAAAAAAAA);
}

bool V1730_wavedump::program(DigitizerSettings &_settings) {
    write32(0xEF2C, 0x4);
    write32(0xEF30, 0xD2);
    write32(0xEF30, 0x0);
    write32(0xEF30, 0x4);
    write32(0xEF30, 0x0);
    write32(0xEF30, 0x0);
    write32(0xEF30, 0x0);
    write32(0xEF30, 0x0);
    write32(0xEF30, 0x0);
    write32(0xEF2C, 0x1);
    write32(0xEF1C, 0x1);
    write32(0xEF00, 0x10);
    write32(0xEF2C, 0x4);
    write32(0xEF30, 0x9F);
    write32(0xEF2C, 0x3);
    write32(0xEF2C, 0x4);
    write32(0xEF30, 0x3);
    write32(0xEF30, 0x0);
    write32(0xEF30, 0x30);
    write32(0xEF30, 0x0);
    write32(0xEF2C, 0x3);
    write32(0xEF24, 0x0);
    write32(0xEF1C, 0x1);
    write32(0xEF00, 0x10);
    write32(0x800C, 0x9);
    write32(0x8020, 0x67);
    write32(0x8114, 0x40);
    write32(0x811C, 0x0);
    write32(0xEF1C, 0x3FF);
    write32(0x8100, 0x0);
    write32(0x810C, 0x80000000);
    write32(0x8110, 0x80000000);
    write32(0x8120, 0x1);
    write32(0x1098, 0xE665);
    write32(0x1080, 0x32);
    write32(0x8000, 0x10);
    write32(0x1084, 0x1);
    write32(0x810C, 0x80000001);
    write32(0x8110, 0x80000000);
    write32(0x8114, 0x40);
    write32(0x1080, 0x6CA);
    write32(0x8100, 0x4);
    write32(0x1080, 0x32);
    write32(0xEF28, 0x0);
    write32(0x8100, 0x0);
    write32(0x8114, 0x40);
    write32(0xEF28, 0x0);
    write32(0x8100, 0x4);
    
    //Enable VME BLT readout
//  write16(REG_READOUT_CONTROL,1<<4);

    readAllRegisters();
    
    return true;
}

void V1730_wavedump::softTrig() {
    write32(REG_SOFTWARE_TRIGGER,0xDEADBEEF);
}

void V1730_wavedump::startAcquisition() {
    write32(REG_ACQUISITION_CONTROL,1<<2);
}

void V1730_wavedump::stopAcquisition() {
    write32(REG_ACQUISITION_CONTROL,0);
}

bool V1730_wavedump::acquisitionRunning() {
    return read32(REG_ACQUISITION_STATUS) & (1 << 2);
}

bool V1730_wavedump::readoutReady() {
    return read32(REG_ACQUISITION_STATUS) & (1 << 3);
}

void V1730_wavedump::readAllRegisters() {
    uint32_t buffer;
    buffer = read32(0x1024); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1124); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1224); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1324); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1424); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1524); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1624); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1724); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1824); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1924); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1A24); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1B24); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1C24); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1D24); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1E24); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1F24); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1028); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1128); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1228); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1328); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1428); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1528); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1628); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1728); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1828); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1928); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1A28); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1B28); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1C28); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1D28); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1E28); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1F28); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1070); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1170); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1270); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1370); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1470); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1570); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1670); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1770); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1870); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1970); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1A70); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1B70); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1C70); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1D70); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1E70); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1F70); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1080); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1180); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1280); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1380); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1480); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1580); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1680); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1780); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1880); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1980); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1A80); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1B80); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1C80); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1D80); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1E80); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1F80); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1084); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1184); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1284); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1384); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1484); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1584); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1684); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1784); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1088); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1188); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1288); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1388); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1488); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1588); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1688); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1788); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1888); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1988); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1A88); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1B88); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1C88); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1D88); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1E88); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1F88); cout << "buffer: " << buffer << endl;
    buffer = read32(0x108C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x118C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x128C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x138C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x148C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x158C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x168C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x178C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x188C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x198C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1A8C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1B8C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1C8C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1D8C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1E8C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1F8C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1098); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1198); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1298); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1398); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1498); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1598); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1698); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1798); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1898); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1998); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1A98); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1B98); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1C98); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1D98); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1E98); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1F98); cout << "buffer: " << buffer << endl;
    buffer = read32(0x10A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x11A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x12A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x13A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x14A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x15A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x16A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x17A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x18A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x19A8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1AA8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1BA8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1CA8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1DA8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1EA8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1FA8); cout << "buffer: " << buffer << endl;
    buffer = read32(0x10EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x11EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x12EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x13EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x14EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x15EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x16EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x17EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x18EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x19EC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1AEC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1BEC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1CEC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1DEC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1EEC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x1FEC); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8000); cout << "buffer: " << buffer << endl;
    buffer = read32(0x800C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8020); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8100); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8104); cout << "buffer: " << buffer << endl;
    buffer = read32(0x810C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8110); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8114); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8118); cout << "buffer: " << buffer << endl;
    buffer = read32(0x811C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8120); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8124); cout << "buffer: " << buffer << endl;
    buffer = read32(0x812C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8138); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8140); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8144); cout << "buffer: " << buffer << endl;
    buffer = read32(0x814C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8168); cout << "buffer: " << buffer << endl;
    buffer = read32(0x816C); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8170); cout << "buffer: " << buffer << endl;
    buffer = read32(0x8178); cout << "buffer: " << buffer << endl;
    buffer = read32(0x81A0); cout << "buffer: " << buffer << endl;
    buffer = read32(0x81B4); cout << "buffer: " << buffer << endl;
    buffer = read32(0x81C4); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF00); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF04); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF08); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF0C); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF10); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF14); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF18); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF1C); cout << "buffer: " << buffer << endl;
    buffer = read32(0xEF20); cout << "buffer: " << buffer << endl;
    buffer = read32(0xF000); cout << "buffer: " << buffer << endl;
    buffer = read32(0xF004); cout << "buffer: " << buffer << endl;
    buffer = read32(0xF008); cout << "buffer: " << buffer << endl;
    buffer = read32(0xF00C); cout << "buffer: " << buffer << endl;
}

bool V1730_wavedump::checkTemps(vector<uint32_t> &temps, uint32_t danger) {
    temps.resize(16);
    bool over = false;
    for (int ch = 0; ch < 16; ch++) {
        temps[ch] = read32(REG_CHANNEL_TEMP|(ch<<8));
        if (temps[ch] >= danger) over = true;
    }
    return over;
}

size_t V1730_wavedump::readoutBLT_evtsz(char *buffer, size_t buffer_size) {
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



V1730_wavedumpDecoder::V1730_wavedumpDecoder(size_t _eventBuffer, V1730_wavedumpSettings &_settings) : eventBuffer(_eventBuffer), settings(_settings) {

    dispatch_index = decode_counter = chanagg_counter = boardagg_counter = 0;
    
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

V1730_wavedumpDecoder::~V1730_wavedumpDecoder() {
    for (size_t i = 0; i < grabs.size(); i++) {
        delete [] grabs[i];
        delete [] patterns[i];
    }
}

void V1730_wavedumpDecoder::decode(Buffer &buf) {
    vector<size_t> lastgrabbed(grabbed);
    
    decode_size = buf.fill();
    cout << settings.getIndex() << " decoding " << decode_size << " bytes." << endl;
    uint32_t *next = (uint32_t*)buf.rptr(), *start = (uint32_t*)buf.rptr();
    while ((size_t)((next = decode_board_agg(next)) - start + 1)*4 < decode_size);
    buf.dec(decode_size);
    decode_counter++;
    
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC,&cur_time);
    double time_int = (cur_time.tv_sec - last_decode_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_decode_time.tv_nsec);
    last_decode_time = cur_time;
    
    for (size_t i = 0; i < idx2chan.size(); i++) {
        cout << "\tch" << idx2chan[i] << "\tev: " << grabbed[i]-lastgrabbed[i] << " / " << (grabbed[i]-lastgrabbed[i])/time_int << " Hz / " << grabbed[i] << " total" << endl;
    }
}

size_t V1730_wavedumpDecoder::eventsReady() {
    size_t grabs = grabbed[0];
    for (size_t idx = 1; idx < grabbed.size(); idx++) {
        if (grabbed[idx] < grabs) grabs = grabbed[idx];
    }
    return grabs;
}

// length, lvdsidx, dsize, nsamples, samples[], strlen, strname[]

void V1730_wavedumpDecoder::dispatch(int nfd, int *fds) {
    
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

void V1730_wavedumpDecoder::writeOut(H5File &file, size_t nEvents) {

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
        
        // FIXME this should be a card-level construct
//      Attribute samples = group.createAttribute("samples",PredType::NATIVE_UINT32,scalar);
//      ival = settings.getRecordLength(idx2chan[i]);
//      samples.write(PredType::NATIVE_UINT32,&ival);
        
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

uint32_t* V1730_wavedumpDecoder::decode_chan_agg(uint32_t *chanagg, uint32_t chn, uint16_t pattern) {

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

uint32_t* V1730_wavedumpDecoder::decode_board_agg(uint32_t *boardagg) {
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
