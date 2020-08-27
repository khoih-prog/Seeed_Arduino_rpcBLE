/*
 * BLERemoteCharacteristic.cpp
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 */

#include "BLERemoteCharacteristic.h"
#include <sstream>

BLERemoteCharacteristic *BLERemoteCharacteristic::_this = NULL;

BLERemoteCharacteristic::BLERemoteCharacteristic(
    uint16_t    decl_handle,  
    uint16_t    properties,   
    uint16_t    value_handle,  
    uint16_t    uuid16,
	BLERemoteService*    pRemoteService
    ) {
	m_handle         = decl_handle;
	m_srvcId16       = uuid16;
	m_uuid           = BLEUUID(m_srvcId16);
	m_end_handle      = value_handle;
	m_pRemoteService = pRemoteService;
	m_notifyCallback = nullptr;
	m_rawData = nullptr;

	retrieveDescriptors(); // Get the descriptors for this characteristic
} // BLERemoteCharacteristic

/**
 * @brief Get the UUID for this characteristic.
 * @return The UUID for this characteristic.
 */
BLEUUID BLERemoteCharacteristic::getUUID() {
	return m_uuid;
} // getUUID

/**
 * @brief Get the handle for this characteristic.
 * @return The handle for this characteristic.
 */
uint16_t BLERemoteCharacteristic::getHandle() {
	return m_handle;
} // getHandle

uint16_t BLERemoteCharacteristic::getendHandle() {
	return m_end_handle;
} // getHandle


/**
 * @brief Get the remote service associated with this characteristic.
 * @return The remote service associated with this characteristic.
 */
BLERemoteService* BLERemoteCharacteristic::getRemoteService() {
	return m_pRemoteService;
} // getRemoteService


/**
 * @brief Does the characteristic support reading?
 * @return True if the characteristic supports reading.
 */
bool BLERemoteCharacteristic::canRead() {
	//return (m_charProp & ESP_GATT_CHAR_PROP_BIT_READ) != 0;
	return true;
} // canRead


/**
 * @brief Does the characteristic support notifications?
 * @return True if the characteristic supports notifications.
 */
bool BLERemoteCharacteristic::canNotify() {
	//return (m_charProp & ESP_GATT_CHAR_PROP_BIT_NOTIFY) != 0;
	return true;
} // canNotify




/**
 * @brief Read the value of the remote characteristic.
 * @return The value of the remote characteristic.
 */
std::string BLERemoteCharacteristic::readValue() {
	// Check to see that we are connected.
	if (!getRemoteService()->getClient()->isConnected()) {
		return std::string();
	}

	m_semaphoreReadCharEvt.take("readValue");

/* 	
	esp_err_t errRc = ::esp_ble_gattc_read_char(
		m_pRemoteService->getClient()->getGattcIf(),
		m_pRemoteService->getClient()->getConnId(),    // The connection ID to the BLE server
		getHandle(),                                   // The handle of this characteristic
		m_auth);		// Security
		
 */
    client_attr_read(m_pRemoteService->getClient()->getConnId(), m_pRemoteService->getClient()->getGattcIf(),getHandle());
	

	// Block waiting for the event that indicates that the read has completed.  When it has, the std::string found
	// in m_value will contain our data.
	m_semaphoreReadCharEvt.wait("readValue");

	return m_value;
} // readValue



/**
 * @brief Register for notifications.
 * @param [in] notifyCallback A callback to be invoked for a notification.  If NULL is provided then we are
 * unregistering a notification.
 * @return N/A.
 */
void BLERemoteCharacteristic::registerForNotify(notify_callback notifyCallback, bool notifications) {
	
	m_notifyCallback = notifyCallback;   // Save the notification callback.

	m_semaphoreRegForNotifyEvt.take("registerForNotify");

	if (notifyCallback != nullptr) {   // If we have a callback function, then this is a registration.
		
		uint8_t val[] = {0x01, 0x00};
		if(!notifications) val[0] = 0x02;
		BLERemoteDescriptor* desc = getDescriptor(BLEUUID((uint16_t)0x2902));
		desc->writeValue(val, 2);
	} // End Register
	else {   // If we weren't passed a callback function, then this is an unregistration.		
		uint8_t val[] = {0x00, 0x00};
		BLERemoteDescriptor* desc = getDescriptor((uint16_t)0x2902);
		desc->writeValue(val, 2);
	} // End Unregister

	m_semaphoreRegForNotifyEvt.wait("registerForNotify");
} // registerForNotify


/**
 * @brief Get the descriptor instance with the given UUID that belongs to this characteristic.
 * @param [in] uuid The UUID of the descriptor to find.
 * @return The Remote descriptor (if present) or null if not present.
 */
BLERemoteDescriptor* BLERemoteCharacteristic::getDescriptor(BLEUUID uuid) {
	std::string v = uuid.toString();
	for (auto &myPair : getRemoteService()->getClient()->m_descriptorMap) {
		if (myPair.first == v) {			
			return myPair.second;
		}
	}
	return nullptr;
} // getDescriptor

void BLERemoteCharacteristic::writeValue(uint8_t newValue, bool response) {
	writeValue(&newValue, 1, response);
} // writeValue

/**
 * @brief Write the new value for the characteristic.
 * @param [in] newValue The new value to write.
 * @param [in] response Do we expect a response?
 * @return N/A.
 */
void BLERemoteCharacteristic::writeValue(std::string newValue, bool response) {
	writeValue((uint8_t*)newValue.c_str(), strlen(newValue.c_str()), response);
} // writeValue


void BLERemoteCharacteristic::writeValue(uint8_t* data, size_t length, bool response) {

	// Check to see that we are connected.
	if (!getRemoteService()->getClient()->isConnected()) {
		return;
	}

	m_semaphoreWriteCharEvt.take("writeValue");
	// Invoke the ESP-IDF API to perform the write.
	/* esp_err_t errRc = ::esp_ble_gattc_write_char(
		m_pRemoteService->getClient()->getGattcIf(),
		m_pRemoteService->getClient()->getConnId(),
		getHandle(),
		length,
		data,
		response?ESP_GATT_WRITE_TYPE_RSP:ESP_GATT_WRITE_TYPE_NO_RSP,
        m_auth
	); */

	client_attr_write(m_pRemoteService->getClient()->getConnId(),m_pRemoteService->getClient()->getGattcIf(),GATT_WRITE_TYPE_REQ,getHandle(),length,(uint8_t *)data);

	m_semaphoreWriteCharEvt.wait("writeValue");
} // writeValue



/**
 * @brief Convert a BLERemoteCharacteristic to a string representation;
 * @return a String representation.
 */
std::string BLERemoteCharacteristic::toString() {
	std::string res = "Characteristic: uuid: " + m_uuid.toString();
	char val[6];
	res += ", handle: ";
	snprintf(val, sizeof(val), "%d", getHandle());
	res += val;
	res += " 0x";
	snprintf(val, sizeof(val), "%04x", getHandle());
	res += val;
//	res += ", props: " + BLEUtils::characteristicPropertiesToString(m_charProp);
	return res;
} // toString

/**
 * @brief Populate the descriptors (if any) for this characteristic.
 */
void BLERemoteCharacteristic::retrieveDescriptors() {

	removeDescriptors();   // Remove any existing descriptors.
	client_all_char_descriptor_discovery(getRemoteService()->getClient()->getConnId(),getRemoteService()->getClient()->getGattcIf(),
                                                m_handle,m_end_handle);

	
    BLERemoteCharacteristic::_this = this;
    m_semaphoregetdescEvt.take("getDescriptor");
	m_haveDescriptor = (m_semaphoregetdescEvt.wait("getDescriptor") == 0);
} // getDescriptors

/**
 * @brief Delete the descriptors in the descriptor map.
 * We maintain a map called m_descriptorMap that contains pointers to BLERemoteDescriptors
 * object references.  Since we allocated these in this class, we are also responsible for deleteing
 * them.  This method does just that.
 * @return N/A.
 */
void BLERemoteCharacteristic::removeDescriptors() {
	// Iterate through all the descriptors releasing their storage and erasing them from the map.
	for (auto &myPair : getRemoteService()->getClient()->m_descriptorMap) {
	   getRemoteService()->getClient()->m_descriptorMap.erase(myPair.first);
	   delete myPair.second;
	}
	getRemoteService()->getClient()->m_descriptorMap.clear();   // Technically not neeeded, but just to be sure.
} // removeCharacteristics

T_APP_RESULT BLERemoteCharacteristic::clientCallbackDefault(T_CLIENT_ID client_id, uint8_t conn_id, void *p_data) {
 
	T_APP_RESULT result = APP_RESULT_SUCCESS;
    T_BLE_CLIENT_CB_DATA *p_ble_client_cb_data = (T_BLE_CLIENT_CB_DATA *)p_data;
	
    switch (p_ble_client_cb_data->cb_type)
    {
    case BLE_CLIENT_CB_TYPE_DISCOVERY_STATE:
        Serial.printf("discov_state:%d\n\r", p_ble_client_cb_data->cb_content.discov_state.state);
        break;
    case BLE_CLIENT_CB_TYPE_DISCOVERY_RESULT:
    {   
        break;	
    }
    case BLE_CLIENT_CB_TYPE_READ_RESULT:
        break;
    case BLE_CLIENT_CB_TYPE_WRITE_RESULT:
        break;
    case BLE_CLIENT_CB_TYPE_NOTIF_IND:{
		if (p_ble_client_cb_data->cb_content.notif_ind.handle != getHandle()) break;
		if (m_notifyCallback != nullptr) {
				
			m_notifyCallback(this,p_ble_client_cb_data->cb_content.notif_ind.p_value, p_ble_client_cb_data->cb_content.notif_ind.value_size,p_ble_client_cb_data->cb_content.notif_ind.notify);
		} // End we have a callback function ...
		break;
	}
	
        break;
    case BLE_CLIENT_CB_TYPE_DISCONNECT_RESULT:
        break;
    default:
        break;
    }
}


