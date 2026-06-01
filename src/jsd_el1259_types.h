#ifndef JSD_EL1259_TYPES_H
#define JSD_EL1259_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "jsd/jsd_common_device_types.h"

#define JSD_EL1259_PRODUCT_CODE (uint32_t)0x04eb3052

#define JSD_EL1259_NUM_DI_CHANNELS 8  ///< 8 digital inputs
#define JSD_EL1259_NUM_DO_CHANNELS 8  ///< 8 digital outputs

/**
 * @brief EL1259 device configuration
 *
 * The EL1259 plain digital-IO mode needs no user parameters; the driver
 * reassigns the process data and enables manual output during PreOp->SafeOp.
 */
typedef struct {
} jsd_el1259_config_t;

/**
 * @brief EL1259 State Data
 *
 * The EL1259 is a multi-timestamp terminal; in this plain digital-IO mode we
 * surface only the per-channel input/output levels (timestamps are ignored).
 */
typedef struct {
  uint8_t input[JSD_EL1259_NUM_DI_CHANNELS];   ///< digital input level (0 or 1)
  uint8_t output[JSD_EL1259_NUM_DO_CHANNELS];  ///< commanded output level (0 or 1)
} jsd_el1259_state_t;

#ifdef __cplusplus
}
#endif

#endif
