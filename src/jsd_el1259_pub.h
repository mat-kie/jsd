#ifndef JSD_EL1259_PUB_H
#define JSD_EL1259_PUB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "jsd/jsd_el1259_types.h"
#include "jsd/jsd_pub.h"

/**
 * @brief Read the EL1259 device state
 *
 * @param self pointer to JSD context
 * @param slave_id id of EL1259 device
 * @return Pointer to EL1259 device state
 */
const jsd_el1259_state_t* jsd_el1259_get_state(jsd_t* self, uint16_t slave_id);

/**
 * @brief Converts raw input PDO data to state data (digital input levels)
 *
 * @param self pointer to JSD context
 * @param slave_id id of EL1259 device
 */
void jsd_el1259_read(jsd_t* self, uint16_t slave_id);

/**
 * @brief Process loop required for proper device function. Injects the
 * commanded output levels into the output PDO each cycle.
 *
 * @param self pointer to JSD context
 * @param slave_id id of EL1259 device
 */
void jsd_el1259_process(jsd_t* self, uint16_t slave_id);

/**
 * @brief Sets a specified output channel level
 *
 * @param self pointer to JSD context
 * @param slave_id id of EL1259 device
 * @param channel specified output channel to command (0-indexed)
 * @param output command level (0 or 1)
 */
void jsd_el1259_write_single_channel(jsd_t* self, uint16_t slave_id,
                                     uint8_t channel, uint8_t output);

/**
 * @brief Sets all output channel levels
 *
 * @param self pointer to JSD context
 * @param slave_id id of EL1259 device
 * @param output command levels (0 or 1) for all output channels
 */
void jsd_el1259_write_all_channels(jsd_t* self, uint16_t slave_id,
                                   uint8_t output[JSD_EL1259_NUM_DO_CHANNELS]);

#ifdef __cplusplus
}
#endif

#endif
