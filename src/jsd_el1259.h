#ifndef JSD_EL1259_H
#define JSD_EL1259_H

#ifdef __cplusplus
extern "C" {
#endif

#include "jsd/jsd.h"

/**
 * @brief Per-channel input TxPDO ("MTI Inputs 1x", CoE 0x1A0B + 0x04*ch).
 *
 * 12 bytes per channel. The digital input level lives in bit 0 of @c status
 * (CoE 0x60n1:09 "Input state"). The remaining fields are part of the
 * multi-timestamp image and are unused in plain digital-IO mode.
 *
 * Note: Struct order matters and must be packed.
 */
typedef struct __attribute__((__packed__)) {
  uint8_t  no_of_input_events;    ///< 0x60n1:01
  uint8_t  status;                ///< bit0 = Input state (0x60n1:09); bit1 = overflow; bits6-7 = cycle counter
  uint8_t  events_in_buffer;      ///< 0x60n1:17
  uint8_t  input_order_feedback;  ///< 0x60n1:18
  uint8_t  input_event_state;     ///< bit0 = Input event state 1 (0x60n1:33)
  uint8_t  reserved[3];
  uint32_t input_event_time;      ///< 0x60n1:65 (unused in plain mode)
} jsd_el1259_di_channel_txpdo_t;

/**
 * @brief Per-channel output RxPDO ("MTO Outputs 1x", CoE 0x1603 + 0x04*ch).
 *
 * 12 bytes per channel. To drive a static output the driver sets bit3
 * "Enable manual operation" (0x70n1:04) and bit1 "Manual output state"
 * (0x70n1:02) in @c ctrl. The remaining fields belong to the multi-timestamp
 * image and are unused in plain digital-IO mode.
 *
 * Note: Struct order matters and must be packed.
 */
typedef struct __attribute__((__packed__)) {
  uint8_t  ctrl;                  ///< bit1 = Manual output state (0x70n1:02); bit3 = Enable manual operation (0x70n1:04)
  uint8_t  output_order_counter;  ///< 0x70n1:09
  uint8_t  no_of_output_events;   ///< 0x70n1:17
  uint8_t  reserved0;
  uint8_t  output_event_state;    ///< bit0 = Output event state 1 (0x70n1:33)
  uint8_t  reserved1[3];
  uint32_t output_event_time;     ///< 0x70n1:65 (unused in plain mode)
} jsd_el1259_do_channel_rxpdo_t;

/**
 * @brief TxPDO struct used to read device data in SOEM IOmap (inputs)
 */
typedef struct __attribute__((__packed__)) {
  jsd_el1259_di_channel_txpdo_t channel[JSD_EL1259_NUM_DI_CHANNELS];
} jsd_el1259_txpdo_t;

/**
 * @brief RxPDO struct used to set device command data in SOEM IOmap (outputs)
 */
typedef struct __attribute__((__packed__)) {
  jsd_el1259_do_channel_rxpdo_t channel[JSD_EL1259_NUM_DO_CHANNELS];
} jsd_el1259_rxpdo_t;

/** @brief Initializes el1259 and registers the PO2SO function
 *
 * @param self pointer JSD context
 * @param slave_id index of device on EtherCAT bus
 * @return true on success, false on failure
 */
bool jsd_el1259_init(jsd_t* self, uint16_t slave_id);

/**
 * @brief Configuration function called by SOEM upon a PreOp to SafeOp state
 * transition. Reassigns the process data to the minimal "plain" per-channel
 * PDOs (8x MTI Inputs 1x + 8x MTO Outputs 1x) and enables manual output mode.
 *
 * @param ecx_context SOEM context pointer
 * @param slave_id index of device on EtherCAT bus
 * @return 1 on success, 0 on failure
 */
int jsd_el1259_PO2SO_config(ecx_contextt* ecx_context, uint16_t slave_id);

/**
 * @brief Checks whether a product code is compatible with EL1259.
 *
 * @param product_code The product code to be checked
 * @return True if the product code is compatible, false otherwise.
 */
bool jsd_el1259_product_code_is_compatible(uint32_t product_code);

#ifdef __cplusplus
}
#endif

#endif
