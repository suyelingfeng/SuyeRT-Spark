/**
 * @file app_tasks.c
 * @brief 闆嗕腑绠＄悊绾跨▼娉ㄥ唽銆佸惎鍔ㄥ弬鏁颁互鍙婄嚎绋嬮棿閫氫俊銆? *
 * 鏁版嵁娴侊細drivers/algorithms -> board_thread -> 鍘熷瓙蹇収 -> GUI/MSH锛? * GUI/MSH -> 璇锋眰鏍囧織 -> board_thread锛汳SH -> 閲嶇粯鏍囧織 -> gui_thread銆? * LVGL 涓嶆槸绾跨▼瀹夊叏搴擄紝鍥犳鍙湁 gui_thread 鍙互璋冪敤 lv_* API銆? */
#include "app_tasks.h"
#include "main.h"
#include "serial_console_ui.h"
#include "st7789_fsmc.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui_navigation.h"
#include <shell.h>
#include <rtthread.h>`n#include "config.h"

/* RT-Thread 涓紭鍏堢骇鏁板瓧瓒婂皬瓒婇珮锛欸UI(18) 鐣ラ珮浜?board(19)锛孎inSH 榛樿 20 鏈€浣庯紝
 * 淇濊瘉鎸夐敭杈撳叆鑳藉強鏃跺緱鍒扮晫闈㈠搷搴斻€備袱绾跨▼鏃堕棿鐗囩浉鍚岋紱GUI 鏍堥渶瀹圭撼 LVGL 缁樺埗
 * 璋冪敤閾撅紝鍥犳鏍堢┖闂存瘮 board 绾跨▼澶т竴鍊嶃€?*/

static struct rt_thread gui_thread;
static struct rt_thread board_thread;
static rt_uint8_t gui_stack[RT_THREAD_RT_THREAD_GUI_STACK_SIZE] __attribute__((aligned(8)));
static rt_uint8_t board_stack[RT_THREAD_RT_THREAD_BOARD_STACK_SIZE] __attribute__((aligned(8)));

/* 鎵€鏈夎法绾跨▼鍏变韩瀵硅薄鍙厑璁稿湪鏈枃浠剁洿鎺ヨ闂€? * 绾跨▼瀹夊叏璁捐锛氬揩鐓?璇锋眰閮芥槸灏忕粨鏋勪綋鐨勬暣浣撴嫹璐濇垨鏍囧織浣嶇疆浣嶏紝涓寸晫鍖烘瀬鐭紝
 * 鏁呯粺涓€鐢?rt_hw_interrupt_disable/enable 鍏充腑鏂繚鎶わ紝鑰屼笉鐢ㄤ簰鏂ラ攣鈥斺€? * 寮€閿€鏈€灏忋€佹棤浼樺厛绾у弽杞闄╋紝涓旇鍐欏弻鏂癸紙board 绾跨▼銆丟UI/MSH 绾跨▼锛? * 閮借兘浠ュ悓鏍锋柟寮忓畨鍏ㄨ闂€?*/
static board_service_snapshot_t shared_snapshot;
static board_service_requests_t pending_requests;
static volatile rt_uint8_t ui_redraw_requested;

/* 鍦ㄥ叧涓柇涓寸晫鍖哄唴鏁翠綋鎷疯礉蹇収锛屼繚璇佽鑰呮嬁鍒扮殑姘歌繙鏄竴浠藉畬鏁翠竴鑷寸殑鏁版嵁銆?*/
static void publish_board_snapshot(const board_service_snapshot_t *source)
{
    rt_base_t level = rt_hw_interrupt_disable();
    shared_snapshot = *source;
    rt_hw_interrupt_enable(level);
}

/* 鍘熷瓙鍦板彇鍑哄苟娓呴浂鍏ㄩ儴寰呭鐞嗚姹傦紝淇濊瘉姣忎釜璇锋眰鍙 board 绾跨▼娑堣垂涓€娆°€?*/
static board_service_requests_t take_board_requests(void)
{
    board_service_requests_t requests;
    rt_base_t level = rt_hw_interrupt_disable();
    requests = pending_requests;
    pending_requests = (board_service_requests_t){0};
    rt_hw_interrupt_enable(level);
    return requests;
}

/* board 鏈嶅姟绾跨▼锛氬懆鏈熼噰闆嗕紶鎰熷櫒/澶栬鏁版嵁锛屽厛娑堣垂璇锋眰鍐嶅彂甯冩渶鏂板揩鐓с€?*/
static void board_thread_entry(void *parameter)
{
    board_service_snapshot_t current;
    RT_UNUSED(parameter);
    board_service_init(&current);
    publish_board_snapshot(&current);
    while (1)
    {
        board_service_requests_t requests = take_board_requests();
        board_service_process(&current, &requests);
        publish_board_snapshot(&current);
        rt_thread_mdelay(board_service_period_ms());
    }
}

/* GUI 绾跨▼锛氬叏宸ョ▼鍞竴鍏佽璋冪敤 lv_* API 鐨勭嚎绋嬶紙LVGL 闈炵嚎绋嬪畨鍏級銆? * 涓婄數鍚庨€愮骇瀹屾垚 LCD鈫扡VGL鈫掓柟鍚戦敭鈫扷I 瀵艰埅鍒濆鍖栵紝浠讳竴绾уけ璐ュ彧鎵撳嵃鏃ュ織
 * 骞堕€€鍑烘湰绾跨▼锛坆oard 绾跨▼鍜?FinSH 浠嶅彲姝ｅ父宸ヤ綔锛屼究浜庢帓闅滐級銆? * 涓诲惊鐜秷璐?MSH 鎻愪氦鐨勯噸缁樿姹傘€侀┍鍔ㄩ〉闈㈠鑸拰 LVGL 瀹氭椂鍣紝
 * 姣?100 鎷嶏紙绾?500 ms锛夌炕杞竴娆＄孩鑹?LED 浣滀负杩愯蹇冭烦銆?*/
static void gui_thread_entry(void *parameter)
{
    lv_group_t *input_group;
    uint32_t heartbeat = 0U;
    RT_UNUSED(parameter);
    serial_console_print_logo();
    rt_kprintf("[1/4] Initialising ST7789 LCD and FSMC...\n");
    if (st7789_init() != 0) { rt_kprintf("LCD FSMC initialisation failed.\n"); return; }
    rt_kprintf("[2/4] LCD ready, initialising LVGL...\n");
    lv_init();
    if (lv_port_disp_init() == RT_NULL) { rt_kprintf("LVGL display registration failed.\n"); return; }
    rt_kprintf("[3/4] LVGL ready, initialising direction keys...\n");
    input_group = lv_port_indev_init();
    if (input_group == RT_NULL) { rt_kprintf("LVGL keypad registration failed.\n"); return; }
    ui_navigation_start(input_group);
    rt_kprintf("[4/4] UI ready. Playing RT-Thread / suye boot animation...\n");
    serial_console_print_quick_help();

    while (1)
    {
        /* MSH/board 鍙兘鎻愪氦娑堟伅锛屾墍鏈?LVGL 鎿嶄綔閮戒覆琛屽彂鐢熷湪杩欓噷銆?*/
        if (ui_redraw_requested != 0U)
        {
            ui_redraw_requested = 0U;
            lv_obj_invalidate(lv_screen_active());
        }
        ui_navigation_process();
        (void)lv_timer_handler();
        if (++heartbeat >= 100U)
        {
            heartbeat = 0U;
            HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
        }
        rt_thread_mdelay(5U);
    }
}

/**
 * @brief 璇锋眰 GUI 绾跨▼閲嶇粯褰撳墠灞忓箷锛堜粎缃爣蹇楋紝闈為樆濉烇級銆? * @note  渚?MSH 绛夊叾浠栫嚎绋嬭皟鐢紱璋冪敤鏂逛笉寰楃洿鎺ユ搷浣滀换浣?LVGL 瀵硅薄銆? */
void app_tasks_request_ui_redraw(void) { ui_redraw_requested = 1U; }

/**
 * @brief 鑾峰彇鏉跨骇鏈嶅姟鐨勬渶鏂板揩鐓э紙鍏充腑鏂复鐣屽尯鍐呮暣浣撴嫹璐濓級銆? * @param snapshot 杈撳嚭缂撳啿鍖猴紱涓?RT_NULL 鏃剁洿鎺ヨ繑鍥烇紝涓嶅彂鐢熸嫹璐濄€? */
void app_tasks_get_board_snapshot(board_service_snapshot_t *snapshot)
{
    rt_base_t level;
    if (snapshot == RT_NULL) return;
    level = rt_hw_interrupt_disable();
    *snapshot = shared_snapshot;
    rt_hw_interrupt_enable(level);
}

/* 鍦ㄥ叧涓柇涓寸晫鍖哄唴缃綅鍗曚釜璇锋眰鏍囧織锛屼緵涓嬫柟涓変釜璇锋眰鎺ュ彛澶嶇敤銆?*/
static void submit_board_request(bool *request)
{
    rt_base_t level = rt_hw_interrupt_disable();
    *request = true;
    rt_hw_interrupt_enable(level);
}

/** @brief 璇锋眰 board 绾跨▼绔嬪嵆鍒锋柊鍏ㄩ儴浼犳劅鍣?澶栬鏁版嵁銆?*/
void app_tasks_request_board_refresh(void) { submit_board_request(&pending_requests.refresh); }
/** @brief 璇锋眰 board 绾跨▼閲嶆柊澶嶄綅骞舵娴?RW007 鏃犵嚎妯″潡銆?*/
void app_tasks_request_rw007_reset(void) { submit_board_request(&pending_requests.rw007_reset); }
/** @brief 璇锋眰 board 绾跨▼鎶婂綋鍓嶅Э鎬侀敋瀹氫负闆剁偣锛堟竻闆剁浉瀵硅搴︼級銆?*/
void app_tasks_request_attitude_zero(void) { submit_board_request(&pending_requests.attitude_zero); }

/**
 * @brief 鍒涘缓骞跺惎鍔?board銆乴vgl 涓や釜搴旂敤绾跨▼锛屾渶鍚庢媺璧?FinSH銆? * @retval RT_EOK 琛ㄧず鍏ㄩ儴鍚姩鎴愬姛锛涘惁鍒欒繑鍥炵涓€涓け璐ユ楠ょ殑閿欒鐮併€? * @note  鐢?rt_application_init() 鍦ㄨ皟搴﹀櫒鍚姩鍓嶈皟鐢紝姝ゅ鐢ㄧ殑鏄潤鎬佺嚎绋嬨€? */
int app_tasks_start(void)
{
    rt_err_t result;
    /* RT-Thread 鏁板瓧瓒婂皬浼樺厛绾ц秺楂橈細GUI=18銆乥oard=19銆丗inSH 榛樿=20銆?*/
    result = rt_thread_init(&board_thread, "board", board_thread_entry, RT_NULL,
                            board_stack, sizeof(board_stack), RT_THREAD_RT_THREAD_BOARD_PRIORITY, RT_THREAD_RT_THREAD_APP_TIME_SLICE);
    if (result == RT_EOK) result = rt_thread_startup(&board_thread);
    if (result != RT_EOK) { rt_kprintf("Board service thread start failed.\n"); return result; }

    result = rt_thread_init(&gui_thread, "lvgl", gui_thread_entry, RT_NULL,
                            gui_stack, sizeof(gui_stack), RT_THREAD_RT_THREAD_GUI_PRIORITY, RT_THREAD_RT_THREAD_APP_TIME_SLICE);
    if (result == RT_EOK) result = rt_thread_startup(&gui_thread);
    if (result != RT_EOK) return result;

    /* FinSH 鑷姩鍙戠幇 shell_commands.c 涓?MSH_CMD_EXPORT_ALIAS 瀵煎嚭鐨勫懡浠ゃ€?*/
    result = (rt_err_t)finsh_system_init();
    if (result == RT_EOK) (void)finsh_set_prompt("suye");
    return result;
}


