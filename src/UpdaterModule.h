#include <Arduino.h>
#include <PicoOTA.h>
#include "OpenKNX.h"
#include "FastCRC.h"

#define INFO_INTERVAL 10000

class UpdaterModule : public OpenKNX::Module
{
	public:
		const std::string name() override;
		const std::string version() override;
		void loop() override;

	private:
        File _file;
        uint8_t *_data;
        uint32_t _size;
        uint _position;
        uint _lastPosition;
        long _lastInfo = 0;
        long _rebootRequested = 0;
        bool _isDownloading = false;
        int _errorCount = 0;
		bool processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength) override;
		bool processFunctionPropertyState(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength) override;
};

//Give your Module a name
//it will be displayed when you use the method log("Hello")
// -> Log     Hello
const std::string UpdaterModule::name()
{
    return "Updater";
}

//You can also give it a version
//will be displayed in Command Infos 
const std::string UpdaterModule::version()
{
    return "0.0dev";
}

void UpdaterModule::loop()
{
    logIndentUp();
    if(_rebootRequested && _rebootRequested + 2000 < millis())
        rp2040.reboot();

    if(_isDownloading && delayCheck(_lastInfo, INFO_INTERVAL))
    {
        _lastInfo = millis();
        logInfoP("Progress: %.2f %% - %i B/s", (_position * 100.0 ) / _size, (_position - _lastPosition) / (INFO_INTERVAL / 1000));
       
        if(_position - _lastPosition == 0)
            _errorCount++;
        else
            _errorCount = 0;

        if(_errorCount > 2)
        {
            logErrorP("Aborting Update.... %i", _errorCount);
            _isDownloading = false;
            _file.close();
            LittleFS.end();
        }
        
        _lastPosition = _position;
    }
    logIndentDown();
}

int counter = 0;

bool UpdaterModule::processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength)
{
    if(objectIndex != 0) return false;

    switch(propertyId)
    {
        case 243:
        {
            logInfoP("Starting Firmware Update");
            _size = data[3] << 24;
            _size |= data[2] << 16;
            _size |= data[1] << 8;
            _size |= data[0];
            _position = 0;
            _lastInfo = millis();
            _lastPosition = 0;
            logIndentUp();
            logInfoP("File Size: %i", _size);
            logIndentDown();
            LittleFS.begin();
            LittleFS.format();
            _file = LittleFS.open("firmware.bin", "w");
            resultLength = 0;
            _isDownloading = true;
            _errorCount = 0;
            return true;
        }
        
        case 244:
        {
            if(!_isDownloading)
            {
                resultData[0] = 0x02;
                resultLength = 1;
                logIndentUp();
                logErrorP("Download aborted");
                logIndentDown();
                return true;
            }

            uint32_t position = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];
            _file.seek(position);

            if(_file.write(data +4, length -4) != length -4)
            {
                resultData[0] = 0x01;
                resultLength = 1;
                logIndentUp();
                logErrorP("Wrong type");
                logIndentDown();
                return true;
            }
            _position = position + length;

            resultData[0] = 0x00;

            FastCRC16 crc16;
            uint16_t crc = crc16.modbus(data +4, length -4);

            resultData[1] = crc >> 8;
            resultData[2] = crc & 0xFF;

            resultLength = 3;
            return true;
        }
        
        case 245:
        {
            logIndentUp();
            logInfoP("Updated finished");
            _isDownloading = false;
            _file.close();
            picoOTA.begin();
            picoOTA.addFile("firmware.bin");
            picoOTA.commit();
            LittleFS.end();
            resultLength = 0;
            _rebootRequested = millis();
            logInfoP("SAVE data to flash");
            openknx.flash.save();
            logInfoP("Device will restart in 2000ms");
            logIndentDown();
            return true;
        }

        case 246:
        {
            logIndentUp();
            logErrorP("Update aborted by KnxUpdater");
            logIndentDown();
            _isDownloading = false;
            _file.close();
            LittleFS.end();
            resultLength = 0;
            return true;
        }
    }
    return false;
}

bool UpdaterModule::processFunctionPropertyState(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength)
{
    return false;
}