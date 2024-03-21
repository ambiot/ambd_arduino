#ifndef _BLE_REMOTECHARACTERISTIC_H_
#define _BLE_REMOTECHARACTERISTIC_H_

#include <Arduino.h>
#include "BLEUUID.h"
#include "BLERemoteDescriptor.h"

#define MAX_NUM_DESC 10
#define CHAR_VALUE_MAX_LEN 230
#define CB_WAIT_TIMEOUT 5000

#ifdef __cplusplus
extern "C" {
#endif

#include "profile_client.h"

#ifdef __cplusplus
}
#endif

// Forward declaration to avoid include loops
class BLEClient;

class BLERemoteCharacteristic {
    public:
        //------------- Configure -------------//
        BLERemoteDescriptor* getDescriptor(const char* uuid);
        BLERemoteDescriptor* getDescriptor(BLEUUID uuid);
        BLEUUID getUUID();
        void setBufferLen(uint16_t max_len);               // Set size of byte array used for containing characteristic data
        uint16_t getBufferLen();
        bool canRead();
        bool canWrite();
        bool canNotify();
        bool canIndicate();
        uint16_t getProperties();

        //---------- Read Char Value ----------//
        String readString();
        uint8_t readData8();
        uint16_t readData16();
        uint32_t readData32();

        //---------- Write Char Value ---------//
        bool writeString(String str, T_GATT_WRITE_TYPE type = GATT_WRITE_TYPE_REQ);
        bool writeString(const char* str, T_GATT_WRITE_TYPE type = GATT_WRITE_TYPE_REQ);
        bool writeData8(uint8_t num);
        bool writeData16(uint16_t num);
        bool writeData32(uint32_t num);
        bool writeData32(int num);

        //------------ Modify Char ------------//
        bool setData(uint8_t* data, uint16_t datalen, T_GATT_WRITE_TYPE type = GATT_WRITE_TYPE_REQ);
        uint16_t getData(uint8_t* data, uint16_t datalen);
        void enableNotifyIndicate(bool notify = 1);
        void disableNotifyIndicate();

        //------------- Callbacks -------------//
        void setNotifyCallback(void (*fCallback) (BLERemoteCharacteristic* chr, uint8_t* data, uint16_t length));

    private:
        BLERemoteCharacteristic(BLEUUID uuid);
        ~BLERemoteCharacteristic();

        bool addDescriptor(BLERemoteDescriptor* newDesc);

        void clientReadResultCallbackDefault(uint8_t conn_id, uint16_t cause, uint16_t handle, uint16_t value_size, uint8_t *p_value);
        void clientWriteResultCallbackDefault(uint8_t conn_id, T_GATT_WRITE_TYPE type, uint16_t handle, uint16_t cause, uint8_t credits);
        T_APP_RESULT clientNotifyIndicateCallbackDefault(uint8_t conn_id, bool notify, uint16_t handle, uint16_t value_size, uint8_t *p_value);

        friend class BLEClient;
        friend class BLERemoteService;

        BLEClient* _pClient = nullptr;
        BLERemoteDescriptor* _descriptorPtrList[MAX_NUM_DESC] = {0};
        uint8_t _descriptorCount = 0;
        BLEUUID _uuid;
        uint16_t _declarationHandle = 0;
        uint16_t _valueHandle = 0;
        uint16_t _properties = 0;

        uint8_t* _data_buf = nullptr;
        uint16_t _data_buf_len = 20;     // Default buffer size of 20 bytes = default MTU 23 - 3 headers
        uint16_t _data_len = 0;         // Length of last received data

        SemaphoreHandle_t _readSemaphore;
        SemaphoreHandle_t _writeSemaphore;

        void (*_pNotifCB)(BLERemoteCharacteristic* chr, uint8_t* data, uint16_t length) = nullptr;
};

#endif
