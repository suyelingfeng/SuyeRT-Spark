# =============================================================================
# STM32F407_RTT 预设断点（GDB 命令文件）
#
# 用法：
#   1. 已安装 Cortex-Debug 扩展：直接选用 launch.json 中的
#      "STM32 Cortex-Debug (带预设断点)" 配置，启动调试时自动 source 本文件。
#   2. 只用 ST 官方扩展（stlinkgdbtarget）：启动调试后在 DEBUG CONSOLE 输入
#         source ${workspaceFolder}/.vscode/breakpoints.gdb
#      （或 -exec source ...，视调试前端而定）。
#
# 说明：函数名断点在符号表中的存在性已用 arm-none-eabi-nm 逐一核对；
#       行号断点对应 Applications/runtime/app_tasks.c 注释完成后的版本，
#       若该文件再次改动请同步更新行号。
# =============================================================================

# ---------- 故障兜底：进任何异常/断言都会先停住 ----------
break Error_Handler          # HAL/时钟配置失败（main.c 死循环前）
break HardFault_Handler      # 硬故障（RT-Thread libcpu 实现，向量表实际入口）
break MemManage_Handler      # 存储器管理故障
break BusFault_Handler       # 总线故障（FSMC 时序/非法地址常触发）
break UsageFault_Handler     # 未定义指令/非法状态
break NMI_Handler            # 不可屏蔽中断
break rt_assert_handler      # RT_ASSERT 断言失败（如 rt_application_init 返回值检查）

# ---------- 启动关键路径 ----------
break rtthread_startup       # main() 把执行权交给 RT-Thread 的起点
break app_tasks_start        # 应用线程注册入口（board/lvgl/FinSH）
break board_thread_entry     # board 服务线程入口（传感器周期采集）
break gui_thread_entry       # GUI 线程入口（LCD/LVGL/UI 逐级初始化）

# ---------- 初始化失败分支（命中即说明对应外设有问题）----------
break app_tasks.c:89         # ST7789 LCD/FSMC 初始化失败
break app_tasks.c:92         # LVGL 显示设备注册失败
break app_tasks.c:95         # 方向键（LVGL 输入设备）注册失败
break app_tasks.c:165        # board 服务线程创建/启动失败

echo \n[breakpoints.gdb] 预设断点已加载（7 个函数断点 + 4 个启动断点 + 4 个失败分支）。\n
