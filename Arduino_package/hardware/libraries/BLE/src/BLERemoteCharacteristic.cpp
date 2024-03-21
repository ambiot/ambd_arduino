#include "BLERemoteCharacteristic.h"
#include "BLEClient.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

//------------- Configure -------------//

BLERemoteDescriptor* BLERemoteCharacteristic::getDescriptor(const char* uuid) {
    return getDescriptor(BLEUUID(uuid));
}

BLERemoteDescriptor* BLERemoteCharacteristic::getDescriptor(BLEUUID uuid) {
    if (_descriptorCount == 0) {
        printf("Characteristic %s error: No descriptors found \n", _uuid.str());
        return nullptr;
    }
    uint8_t i;
    for (i = 0; i < _descriptorCount; i++) {
        if (_descriptorPtrList[i]->getUUID() == uuid) {
            return _descriptorPtrList[i];
        }
    }
    return nullptr; // Target not found, return nullptr
}

BLEUUID BLERemoteCharacteristic::getUUID() {
    return _uuid;
}

void BLERemoteCharacteristic::setBufferLen(uint16_t max_len) {
    if (max_len > CHAR_VALUE_MAX_LEN) {
        printf ("Characteristic %s error: requested buffer size too large, maximum of %d \n", _uuid.str(), CHAR_VALUE_MAX_LEN);
    } else {
        _data_buf = (uint8_t*) realloc(_data_buf, max_len*sizeof(uint8_t));
        if (_data_buf == NULL) {
            printf("Characteristic %s error: Not enough memory to set buffer length \n", _uuid.str());
            _data_buf_len = 0;
        } else {
            _data_buf_len = max_len;
        }
    }
}

uint16_t BLERemoteCharacteristic::getBufferLen() {
    return _data_buf_len;
}

bool BLERemoteCharacteristic::canRead() {
    return (_properties & GATT_CHAR_PROP_READ);
}

bool BLERemoteCharacteristic::canWrite() {
    return ((_properties & GATT_CHAR_PROP_WRITE) || (_properties & GATT_CHAR_PROP_WRITE_NO_RSP));
}

bool BLERemoteCharacteristic::canNotify() {
    return (_properties & GATT_CHAR_PROP_NOTIFY) ;
}

bool BLERemoteCharacteristic::canIndicate() {
    return (_properties & GATT_CHAR_PROP_INDICATE) ;
}

uint16_t BLERemoteCharacteristic::getProperties() {
    return _properties;
}

//--------- Read Char Value --------//

String BLERemoteCharacteristic::readString() {
    char datastring[_data_buf_len + 1] = {0};
    getData((uint8_t*)datastring, sizeof(datastring));
    return String(datastring);
}

uint8_t BLERemoteCharacteristic::readData8() {
    uint8_t num = 0;
    getData(&num, sizeof(num));
    return num;
}

uint16_t BLERemoteCharacteristic::readData16() {
    uint16_t num = 0;
    getData((uint8_t*) &num, sizeof(num));
    return num;
}

uint32_t BLERemoteCharacteristic::readData32() {
    uint32_t num = 0;
    getData((uint8_t*) &num, sizeof(num));
    return num;
}

//--------- Write Char Value --------//

bool BLERemoteCharacteristic::writeString(String str, T_GATT_WRITE_TYPE type) {
    return writeString(str.c_str(), type);
}

bool BLERemoteCharacteristic::writeString(const char* str, T_GATT_WRITE_TYPE type) {
    return setData((uint8_t*) str, strlen(str), type);
}

bool BLERemoteCharacteristic::writeData8(uint8_t num) {
    return setData((uint8_t*) &num, sizeof(num));
}

bool BLERemoteCharacteristic::writeData16(uint16_t num) {
    return setData((uint8_t*) &num, sizeof(num));
}

bool BLERemoteCharacteristic::writeData32(uint32_t num) {
    return setData((uint8_t*) &num, sizeof(num));
}

bool BLERemoteCharacteristic::writeData32(int num) {
    return setData((uint8_t*) &num, sizeof(num));
}

//------------ Modify Char ------------//

bool BLERemoteCharacteristic::setData(uint8_t* data, uint16_t datalen, T_GATT_WRITE_TYPE type) {
    if (!canWrite()) {
        printf("Characteristic %s error: write not permitted \n", _uuid.str());
        return false;
    }
    // Check if device is still connected
    if (!_pClient->connected()) {
        printf("Characteristic %s error: client not connected \n", _uuid.str());
        return false;
    }
    // Attempt to write
    if (client_attr_write(_pClient->getConnId(), _pClient->getClientId(),  type, _valueHandle, datalen, data) == GAP_CAUSE_SUCCESS) {
        // Check for write callback semaphore indicating data write successful
        if (xSemaphoreTake(_writeSemaphore, CB_WAIT_TIMEOUT/portTICK_PERIOD_MS) != pdTRUE) {
            printf("Characteristic %s error: set data timeout \n", _uuid.str());
            return false;
        }
    } else {
        printf("Characteristic %s error: client_attr_write failed \n", _uuid.str());
        return false;
    }
    return true;
}

uint16_t BLERemoteCharacteristic::getData(uint8_t* data, uint16_t datalen) {
    if (!canRead()) {
        printf("Characteristic %s error: read not permitted \n", _uuid.str());
        return 0;
    }
    // Check if device is still connected
    if (!_pClient->connected()) {
        printf("Characteristic %s error: client not connected \n", _uuid.str());
        return 0;
    }
    // Attempt to read
    if (client_attr_read(_pClient->getConnId(), _pClient->getClientId(), _valueHandle) == GAP_CAUSE_SUCCESS) {
        // Check for read callback semaphore indicating new data received
        if (xSemaphoreTake(_readSemaphore, CB_WAIT_TIMEOUT/portTICK_PERIOD_MS) != pdTRUE) {
            printf("Characteristic %s error: get data timeout \n", _uuid.str());
        } else {
            // Copy new data, up to the smaller data buffer size
            if (datalen > _data_buf_len) {
                memcpy(data, _data_buf, _data_buf_len);
                return _data_buf_len;
            } else {
                memcpy(data, _data_buf, datalen);
                return datalen;
            }
        }
    } else {
        printf("Characteristic %s error: client_attr_read failed \n", _uuid.str());
    }
    return 0;
}

void BLERemoteCharacteristic::enableNotifyIndicate(bool notify) {
    BLERemoteDescriptor* pdesc = getDescriptor("2902");
    if (pdesc == nullptr) {
        printf("Characteristic %s error: CCCD descriptor not found \n", _uuid.str());
        return;
    }
    uint8_t val[2] = {0x01, 0x00};
    if (!notify) {
        val[0] = 0x02;
    }
    pdesc->setData(val, 2);
}

void BLERemoteCharacteristic::disableNotifyIndicate() {
    BLERemoteDescriptor* pdesc = getDescriptor("2902");
    if (pdesc == nullptr) {
        printf("Characteristic %s error: CCCD descriptor not found \n", _uuid.str());
        return;
    }
    uint8_t val[2] = {0x00, 0x00};
    pdesc->setData(val, 2);
}

//------------- Callbacks -------------//

void BLERemoteCharacteristic::setNotifyCallback(void (*fCallback) (BLERemoteCharacteristic* chr, uint8_t* data, uint16_t length)) {
    _pNotifCB  = fCallback;
}

//---------- Private Methods ----------//

BLERemoteCharacteristic::BLERemoteCharacteristic(BLEUUID uuid) {
    _uuid = uuid;
    _data_buf = (uint8_t*)malloc(_data_buf_len*sizeof(uint8_t));
    _readSemaphore = xSemaphoreCreateBinary();
    _writeSemaphore = xSemaphoreCreateBinary();
}

BLERemoteCharacteristic::~BLERemoteCharacteristic() {
    uint8_t i;
    for (i = 0; i < _descriptorCount; i++) {
        delete _descriptorPtrList[i];
        _descriptorPtrList[i] = nullptr;
    }
    free(_data_buf);
    vSemaphoreDelete(_readSemaphore);
    vSemaphoreDelete(_writeSemaphore);
}

bool BLERemoteCharacteristic::addDescriptor(BLERemoteDescriptor* newDesc) {
    if (_descriptorCount < (MAX_NUM_DESC)) {
        _descriptorPtrList[_descriptorCount] = newDesc;
        _descriptorCount += 1;
        return true;
    }
    printf("Characteristic %s error: Maximum number of descriptors per characteristic reached \n", _uuid.str());
    return false;
}

void BLERemoteCharacteristic::clientReadResultCallbackDefault(uint8_t conn_id, uint16_t cause, uint16_t handle, uint16_t value_size, uint8_t *p_value) {
    if (handle == _valueHandle) {
        if (cause == GAP_SUCCESS) {
            if (value_size > _data_buf_len) {
                printf("Characteristic %s error: Buffer size insufficient for data size of %d bytes \n", _uuid.str(), _data_buf_len);
            } else {
                memset(_data_buf, 0, _data_buf_len);
                memcpy(_data_buf, p_value, value_size);
                _data_len = value_size;
            }
            xSemaphoreGive(_readSemaphore);
        }
    } else {
        for (uint8_t i = 0; i < _descriptorCount; i++) {
            if (handle == _descriptorPtrList[i]->_declarationHandle) {
                _descriptorPtrList[i]->clientReadResultCallbackDefault(conn_id, cause, handle, value_size, p_value);
                break;
            }
        }
    }
}

void BLERemoteCharacteristic::clientWriteResultCallbackDefault(uint8_t conn_id, T_GATT_WRITE_TYPE type, uint16_t handle, uint16_t cause, uint8_t credits) {
    if (handle == _valueHandle) {
        if (cause == GAP_SUCCESS) {
            xSemaphoreGive(_writeSemaphore);
        }
    } else {
        for (uint8_t i = 0; i < _descriptorCount; i++) {
            if (handle == _descriptorPtrList[i]->_declarationHandle) {
                _descriptorPtrList[i]->clientWriteResultCallbackDefault(conn_id, type, handle, cause, credits);
                break;
            }
        }
    }
}

T_APP_RESULT BLERemoteCharacteristic::clientNotifyIndicateCallbackDefault(uint8_t conn_id, bool notify, uint16_t handle, uint16_t value_size, uint8_t *p_value) {
    T_APP_RESULT app_result = APP_RESULT_APP_ERR;
    if (handle != _valueHandle) {
        printf("Characteristic %s error: Handle %d mismatch in notify/indicate callback \n", _uuid.str(), handle);
        return app_result;
    } else if (value_size > _data_buf_len) {
        printf("Characteristic %s error: Buffer size insufficient for data size of %d bytes \n", _uuid.str(), _data_buf_len);
        return app_result;
    } else {
        /*/
        memset(_data_buf, 0, _data_buf_len);
        memcpy(_data_buf, p_value, value_size);
        _data_len = value_size;
        //*/
        if (_pNotifCB != nullptr) {
            _pNotifCB(this, p_value, value_size);
        }
    }
    return APP_RESULT_SUCCESS;
    (void)conn_id;
    (void)notify;
}

