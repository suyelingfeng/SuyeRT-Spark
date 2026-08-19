/**
 * @file board.h
 * @brief BSP 灞傚澶栫殑鑱氬悎澶存枃浠讹紝鏄?Core/ 涓庡簲鐢ㄥ眰杩涘叆鏉跨骇鏀寔鐨勭粺涓€鍏ュ彛銆? *
 * 鑱氬悎鍏崇郴锛氭湰澶存枃浠跺紩鍏?HAL 鎬诲ご锛坰tm32f4xx_hal.h锛変笌 RT-Thread 鍐呮牳澶? *锛坮tthread.h锛夛紝浣垮寘鍚€呭悓鏃跺叿澶囪闂璁鹃┍鍔ㄥ拰鍐呮牳 API 鐨勮兘鍔涳紱
 * 涓嬫父鍚?BSP 妯″潡锛坙cd/ 鏄剧ず灞忋€乨rivers/ 浼犳劅鍣ㄤ笌鏃犵嚎銆乻ervices/ 鏉跨骇涓氬姟銆? * algorithms/ 绠楁硶锛夊悇鑷彁渚涚嫭绔嬪ご鏂囦欢锛屼笉鍦ㄦ閲嶅鍖呭惈锛岄伩鍏嶅惊鐜緷璧栥€? * 鏁版嵁娴侊細main() -> rtthread_startup() -> 鍐呮牳鍒濆鍖?-> 搴旂敤绾跨▼璋冪敤
 * services/drivers/lcd 瀹屾垚纭欢鍔熻兘銆? */
#ifndef BSP_BOARD_H__
#define BSP_BOARD_H__

#include "stm32f4xx_hal.h"
#include <rtthread.h>
#include "config.h"

/* ============================================================================
 * Unified Error Handling
 * ========================================================================== */

/**
 * @brief Unified error code enumeration for board-level drivers and services.
 *
 * All driver functions should return this type (or bool for init functions) to provide
 * consistent error reporting across the codebase. This eliminates ambiguity when calling
 * functions that return 0/1/-1 or true/false with different semantics.
 */
typedef enum
{
    BOARD_OK = 0,              /**< Operation successful. */
    BOARD_ERROR_PARAM = -1,    /**< Invalid parameter (NULL pointer, out of range). */
    BOARD_ERROR_I2C = -2,      /**< I2C communication failure (timeout, NACK, CRC error). */
    BOARD_ERROR_TIMEOUT = -3,  /**< Operation timeout (device not responding in time). */
    BOARD_ERROR_DEVICE = -4    /**< Device error (WHO_AM_I mismatch, internal fault). */
} board_error_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RT-Thread 鍚姩鍏ュ彛锛岀敱 main() 鍦ㄥ畬鎴愬熀纭€澶栬鍒濆鍖栧悗璋冪敤銆? *
 * 璐熻矗鍫嗐€佸畾鏃跺櫒銆佽皟搴﹀櫒绛夊唴鏍稿垵濮嬪寲骞跺垱寤哄簲鐢ㄧ嚎绋嬶紱
 * 璋冨害鍣ㄥ惎鍔ㄥ悗姝ｅ父涓嶄細杩斿洖锛岃瑙?rtthread_startup.c銆? */
void rtthread_startup(void);

#ifdef __cplusplus
}
#endif

#endif

