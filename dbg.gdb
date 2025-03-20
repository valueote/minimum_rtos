file build/minimum_rtos.elf
target remote localhost:3333
#layout src
b main
b led_right
b led_close
b idle_task
b vTaskSwitchContext
