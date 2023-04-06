#include <Arduino.h>
#include <PicoOTA.h>
#include "OpenKNX.h"
#include "FastCRC.h"

#define INFO_INTERVAL 10000

class UpdateModule : public OpenKNX::Module
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
//it will be desplayed when you use the method log("Hello")
// -> Log     Hello
const std::string UpdateModule::name()
{
    return "UpdateModule";
}

//You can also give it a version
//will be displayed in Command Infos 
const std::string UpdateModule::version()
{
    return "0.0dev";
}

void UpdateModule::loop()
{
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

        if(_position - _lastPosition == 0)
            _errorCount++;

        if(_errorCount > 3)
        {
            logErrorP("Aborting Update...");
            _isDownloading = false;
            _file.close();
        }
    }
}

bool UpdateModule::processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength)
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
            logInfoP("File Size: %i", _size);
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
                logErrorP("Download aborted");
                return true;
            }

            if(_file.write(data, length) != length)
            {
                resultData[0] = 0x01;
                resultLength = 1;
                logErrorP("Wrong type");
                return true;
            }
            _position += length;

            resultData[0] = 0x00;

            FastCRC16 crc16;
            uint16_t crc = crc16.modbus(data, length);

            resultData[1] = crc >> 8;
            resultData[2] = crc & 0xFF;

            resultLength = 3;
            return true;
        }
        
        case 245:
        {
            _isDownloading = false;
            _file.close();
            picoOTA.begin();
            picoOTA.addFile("firmware.bin");
            picoOTA.commit();
            LittleFS.end();
            resultLength = 0;
            _rebootRequested = millis();
            return true;
        }
    }
    return false;
}

bool UpdateModule::processFunctionPropertyState(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength)
{
    return false;
}