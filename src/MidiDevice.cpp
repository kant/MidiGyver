#include "MidiDevice.h"

#include "ops/values.h"
#include "ops/strings.h"

#include "types/Color.h"
#include "types/Vector.h"

//status bytes
const unsigned char NOTE_OFF = 0x80;
const unsigned char NOTE_ON = 0x90;
const unsigned char KEY_PRESSURE = 0xA0;
const unsigned char CONTROLLER_CHANGE = 0xB0;
const unsigned char PROGRAM_CHANGE = 0xC0;
const unsigned char CHANNEL_PRESSURE = 0xD0;
const unsigned char PITCH_BEND = 0xE0;
const unsigned char SYSTEM_EXCLUSIVE = 0xF0;
const unsigned char SONG_POSITION = 0xF2;
const unsigned char SONG_SELECT = 0xF3;
const unsigned char TUNE_REQUEST = 0xF6;
const unsigned char END_OF_SYSEX = 0xF7;
const unsigned char TIMING_TICK = 0xF8;
const unsigned char START_SONG = 0xFA;
const unsigned char CONTINUE_SONG = 0xFB;
const unsigned char STOP_SONG = 0xFC;
const unsigned char ACTIVE_SENSING = 0xFE;
const unsigned char SYSTEM_RESET = 0xFF;

MidiDevice::MidiDevice(Context* _ctx, const std::string& _deviceName, size_t _midiPort) {
    try {
        midiIn = new RtMidiIn();
    } catch(RtMidiError &error) {
        error.printMessage();
    }
    midiIn->openPort(_midiPort);

    try {
        midiOut = new RtMidiOut();
        midiOut->openPort(_midiPort);
    } catch(RtMidiError &error) {
        error.printMessage();
        midiOut = nullptr;
    }

    midiPort = _midiPort;
    midiName = midiIn->getPortName(_midiPort);
    stringReplace(midiName, '_');

    deviceName = _deviceName;
    ctx = _ctx;

    midiIn->setCallback(onMidi, this);
    midiIn->ignoreTypes(false, false, true);

    // Load default values for toggles
    if ( ctx->config["in"][deviceName].IsMap() ) {
        for (YAML::const_iterator it = ctx->config["in"][deviceName].begin(); it != ctx->config["in"][deviceName].end(); ++it) {
            std::string key = it->first.as<std::string>();       // <- key
            if (it->second["value"] && it->second["type"])
                if (it->second["type"].as<std::string>() == "toggle")
                    setLED(toInt(key), it->second["value"].as<bool>());
        }
    }
}

MidiDevice::~MidiDevice() {
    delete this->midiIn;
}

void extractHeader(std::vector<unsigned char>* _message, std::string& _type, int& _bytes, unsigned char& _channel) {
    unsigned char status = 0;
    int j = 0;

    if ((_message->at(0) & 0xf0) != 0xf0) {
        _channel = _message->at(0) & 0x0f;
        status = _message->at(0) & 0xf0;
    }
    else {
        _channel = 0;
        status = _message->at(0);
    }

    switch(status) {
        case NOTE_OFF:
            _type = "note_off";
            _bytes = 2;
            break;

        case NOTE_ON:
            _type = "note_on";
            _bytes = 2;
            break;

        case KEY_PRESSURE:
            _type = "key_pressure";
            _bytes = 2;
            break;

        case CONTROLLER_CHANGE:
            _type = "controller_change";
            _bytes = 2;
            break;

        case PROGRAM_CHANGE:
            _type = "program_change";
            _bytes = 2;
            break;

        case CHANNEL_PRESSURE:
            _type = "channel_pressure";
            _bytes = 2;
            break;

        case PITCH_BEND:
            _type = "pitch_bend";
            _bytes = 2;
            break;

        case SYSTEM_EXCLUSIVE:
            if(_message->size() == 6) {
                unsigned int type = _message->at(4);
                if (type == 1)
                    _type = "mmc_stop";
                else if(type == 2)
                    _type = "mmc_play";
                else if(type == 4)
                    _type = "mmc_fast_forward";
                else if(type == 5)
                    _type = "mmc_rewind";
                else if(type == 6)
                    _type = "mmc_record";
                else if(type == 9)
                    _type = "mmc_pause";
            }
            _bytes = 0;
            break;

        case SONG_POSITION:
            _type = "song_position";
            _bytes = 2;
            break;

        case SONG_SELECT:
            _type = "song_select";
            _bytes = 2;
            break;

        case TUNE_REQUEST:
            _type = "tune_request";
            _bytes = 2;
            break;

        case TIMING_TICK:
            _type = "timing_tick";
            _bytes = 0;
            break;

        case START_SONG:
            _type = "start_song";
            _bytes = 0;
            break;

        case CONTINUE_SONG:
            _type = "continue_song";
            _bytes = 0;
            break;

        case STOP_SONG:
            _type = "stop_song";
            _bytes = 0;
            break;

        default:
            _type = "";
            _bytes = 0;
            break;
    }

    if (status == NOTE_ON && _message->at(2) == 0) {
        _type = "note_off";
    }
}

void MidiDevice::onMidi(double _deltatime, std::vector<unsigned char>* _message, void* _userData) {
    unsigned int nBytes = 0;
    try {
        nBytes = _message->size();
    } 
    catch(RtMidiError &error) {
        error.printMessage();
        exit(EXIT_FAILURE);
    }

    MidiDevice *device = static_cast<MidiDevice*>(_userData);

    std::string type;
    unsigned char channel;
    int bytes;
    extractHeader(_message, type, bytes, channel);

    size_t key = _message->at(1);
    size_t value = _message->at(2);

    device->parseMessage( key, value );
}

void MidiDevice::setLED(size_t _key, bool _value) {
    if (midiOut) {
        std::vector<unsigned char> msg;
        msg.push_back( 0xB0 );
        msg.push_back( _key );
        msg.push_back( _value? 127 : 0 );
        midiOut->sendMessage( &msg );
    }
}

bool MidiDevice::parseMessage(size_t _key, size_t _value) {
    std::string key = toString(_key);
    std::string name = key;

    // If there is no device associated finish
    if ( ctx->config["in"][deviceName].IsNull() )
        return false;

    // 
    if ( ctx->config["in"][deviceName][key] ) {
        std::string type = "unknown";

        if ( ctx->config["in"][deviceName][key]["type"] )
            type =  ctx->config["in"][deviceName][key]["type"].as<std::string>();

        if ( type == "button" ) {
            bool value = (int)_value == 127;
            ctx->config["in"][deviceName][key]["value"] = value;
            setLED(_key, value );
            return ctx->updateKey(deviceName, key);
        }
        else if ( type == "toggle" ) {
            
            if ((int)_value == 127) {
                bool value = false;
                
                if (ctx->config["in"][deviceName][key]["value"])
                    value = ctx->config["in"][deviceName][key]["value"].as<bool>();
                ctx->config["in"][deviceName][key]["value"] = !value;
                setLED(_key, !value );
                return ctx->updateKey(deviceName, key);
            }
        }
        else if ( type == "states" ) {
            int value = (int)_value;
            std::string value_str = toString(value);

            if ( ctx->config["in"][deviceName][key]["map"] ) {
                if ( ctx->config["in"][deviceName][key]["map"].IsSequence() ) {
                    float total = ctx->config["in"][deviceName][key]["map"].size();

                    if (value == 127.0f) {
                        value_str = ctx->config["in"][deviceName][key]["map"][total-1].as<std::string>();
                    } 
                    else {
                        size_t index = (value / 127.0f) * (ctx->config["in"][deviceName][key]["map"].size());
                        value_str = ctx->config["in"][deviceName][key]["map"][index].as<std::string>();
                    } 
                }
            }
            
            ctx->config["in"][deviceName][key]["value"] = value_str;
            return ctx->updateKey(deviceName, key);
        }
        else if ( type == "scalar" ) {
            float value = (float)_value / 127.0f;

            if ( ctx->config["in"][deviceName][key]["map"] ) {
                if ( ctx->config["in"][deviceName][key]["map"].IsSequence() ) {
                    if (ctx->config["in"][deviceName][key]["map"].size() > 1) {
                        float total = ctx->config["in"][deviceName][key]["map"].size() - 1;

                        size_t i_low = value * total;
                        size_t i_high = std::min(i_low + 1, size_t(total));
                        float pct = (value * total) - (float)i_low;
                        value = lerp(   ctx->config["in"][deviceName][key]["map"][i_low].as<float>(),
                                        ctx->config["in"][deviceName][key]["map"][i_high].as<float>(),
                                        pct );
                    }

                }
            }
            
            ctx->config["in"][deviceName][key]["value"] = value;
            return ctx->updateKey(deviceName, key);
        }
        else if ( type == "vector" ) {
            float pct = (float)_value / 127.0f;
            Vector value = Vector(0.0, 0.0, 0.0);

            if ( ctx->config["in"][deviceName][key]["map"] ) {
                if ( ctx->config["in"][deviceName][key]["map"].IsSequence() ) {
                    if (ctx->config["in"][deviceName][key]["map"].size() > 1) {
                        float total = ctx->config["in"][deviceName][key]["map"].size() - 1;

                        size_t i_low = pct * total;
                        size_t i_high = std::min(i_low + 1, size_t(total));

                        value = lerp(   ctx->config["in"][deviceName][key]["map"][i_low].as<Vector>(),
                                        ctx->config["in"][deviceName][key]["map"][i_high].as<Vector>(),
                                        (pct * total) - (float)i_low );
                    }

                }
            }
            
            ctx->config["in"][deviceName][key]["value"] = value;
            return ctx->updateKey(deviceName, key);
        }
        else if ( type == "color" ) {
            float pct = (float)_value / 127.0f;
            Color value = Color(0.0, 0.0, 0.0);

            if ( ctx->config["in"][deviceName][key]["map"] ) {
                if ( ctx->config["in"][deviceName][key]["map"].IsSequence() ) {
                    if (ctx->config["in"][deviceName][key]["map"].size() > 1) {
                        float total = ctx->config["in"][deviceName][key]["map"].size() - 1;

                        size_t i_low = pct * total;
                        size_t i_high = std::min(i_low + 1, size_t(total));

                        value = lerp(   ctx->config["in"][deviceName][key]["map"][i_low].as<Color>(),
                                        ctx->config["in"][deviceName][key]["map"][i_high].as<Color>(),
                                        (pct * total) - (float)i_low );
                    }

                }
            }
            
            ctx->config["in"][deviceName][key]["value"] = value;
            return ctx->updateKey(deviceName, key);
        }
    }

    // else {
    //     int value = (int)_value;
    //     if (osc)
    //         sendValue(oscAddress, oscPort, oscFolder + key, value);

    //     if (csv)
    //         std::cout << key << "," << value << std::endl;
    // }

    return false;
}


