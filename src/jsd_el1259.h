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
 * 12 bytes per channel. To drive a static output the driver sets bit1 "Manual
 * output state" (0x70n1:02) in @c ctrl. Manual mode itself is enabled once via
 * the SDO 0x80n1:02 ("Enable manual operation") during PO2SO config -- it is
 * not a PDO control bit. The remaining fields belong to the multi-timestamp
 * image and are unused in plain digital-IO mode.
 *
 * Note: Struct order matters and must be packed.
 */
typedef struct __attribute__((__packed__)) {
  uint8_t  ctrl;                  ///< bit1 = Manual output state (0x70n1:02); bits3-7 reserved
  uint8_t  output_order_counter;  ///< 0x70n1:09
  uint8_t  no_of_output_events;   ///< 0x70n1:17
  uint8_t  reserved0;
  uint8_t  output_event_state;    ///< bit0 = Output event state 1 (0x70n1:33)
  uint8_t  reserved1[3];
  uint32_t output_event_time;     ///< 0x70n1:65 (unused in plain mode)
} jsd_el1259_do_channel_rxpdo_t;

/**
 * @brief Per-channel "MTO Inputs" companion TxPDO (CoE 0x1A00 + ch).
 *
 * 4 bytes per channel. Carries the digital-OUTPUT status (short circuit, output
 * state, cycle counter) and is unused in plain digital-IO mode. It is part of
 * the device's valid SM3 input image, however, and must be mapped alongside
 * "MTI Inputs" -- mapping "MTI Inputs" alone makes the slave reject the input
 * mapping (AL status code 0x0024 "Invalid input mapping").
 *
 * Note: Struct order matters and must be packed.
 */
typedef struct __attribute__((__packed__)) {
  uint8_t status;                  ///< bit2 = Output state (0x60n0:03)
  uint8_t status_hi;               ///< remainder incl. bits6-7 cycle counter
  uint8_t output_order_feedback;   ///< 0x60n0:17
  uint8_t events_in_output_buffer; ///< 0x60n0:18
} jsd_el1259_mto_input_txpdo_t;

/**
 * @brief Per-channel "MTI Outputs" companion RxPDO (CoE 0x1620 + ch).
 *
 * 4 bytes per channel. Carries digital-INPUT control (input buffer reset, input
 * order counter) and is unused in plain digital-IO mode. It is the SM2
 * counterpart to the "MTO Inputs" companion and is mapped so the output image
 * is complete and symmetric (avoids AL status 0x0025 once the input side is
 * valid).
 *
 * Note: Struct order matters and must be packed.
 */
typedef struct __attribute__((__packed__)) {
  uint8_t ctrl;                ///< bit0 = Input buffer reset (0x70n0:01)
  uint8_t reserved0;
  uint8_t input_order_counter; ///< 0x70n0:17
  uint8_t reserved1;
} jsd_el1259_mti_output_rxpdo_t;

/**
 * @brief TxPDO struct used to read device data in SOEM IOmap (inputs, SM3).
 *
 * Mirrors the device's valid input image in assignment order, grouped by
 * family: 8x "MTO Inputs" (output status) followed by 8x "MTI Inputs 1x" (which
 * carry the digital input level). Total 8*4 + 8*12 = 128 bytes.
 */
typedef struct __attribute__((__packed__)) {
  jsd_el1259_mto_input_txpdo_t  mto_input[JSD_EL1259_NUM_DI_CHANNELS];
  jsd_el1259_di_channel_txpdo_t mti_input[JSD_EL1259_NUM_DI_CHANNELS];
} jsd_el1259_txpdo_t;

/**
 * @brief RxPDO struct used to set device command data in SOEM IOmap (outputs, SM2).
 *
 * Mirrors the device's valid output image in assignment order, grouped by
 * family: 8x "MTO Outputs 1x" (which carry the manual output level) followed by
 * 8x "MTI Outputs" (input control). Total 8*12 + 8*4 = 128 bytes.
 */
typedef struct __attribute__((__packed__)) {
  jsd_el1259_do_channel_rxpdo_t mto_output[JSD_EL1259_NUM_DO_CHANNELS];
  jsd_el1259_mti_output_rxpdo_t mti_output[JSD_EL1259_NUM_DO_CHANNELS];
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
 * transition. Reassigns the process data to a valid digital-IO image -- SM3 =
 * 8x MTO Inputs + 8x MTI Inputs 1x, SM2 = 8x MTO Outputs 1x + 8x MTI Outputs --
 * and enables manual output mode. Both PDO families must be present per SM or
 * the slave rejects the mapping (AL status 0x0024/0x0025).
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
