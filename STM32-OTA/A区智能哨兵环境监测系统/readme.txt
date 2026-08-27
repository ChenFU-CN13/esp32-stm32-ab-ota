HAL库时钟源采用定时器7
延时函数时钟采用定时器6(目前仅采用计时)
rtos时钟源采用SYSTEMHCLK

不可以使用cubemx进行配置了
否则会导致覆盖，工程烂掉

默认MDK优化最高级+编译器v5.08（编译器等级越高越掉帧）

注意：工程使用的是v8.3.11的LVGL

STM32ZGT6:192KB RAM; 1 MB ROM

1.这个屏幕使用双缓存白屏卡死的原因：
如果是需要驱动完整的800*480的屏幕，需要很大的RAM
首先分配加大系统堆栈内存  Stack_Size		EQU     0x1000；Heap_Size      EQU     0x4000

系统栈空间一般不用太大：因为仅用于中断服务函数，中断中一般不做复杂操作，4KB 完全够用。
与 RTOS 任务无关：RTOS 启动后（调用 osKernelStart()），
系统栈仅用于中断，所有任务的运行都使用自己独立的 “任务栈”，不再依赖这个 Stack_Size

为什么要调大系统堆内存
系统堆内存是RTOS1任务栈空间   是  从"系统堆空间"中分配的
比如Heap_Size      EQU     0x4000 = 16KB

任务栈空间：
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 8, 等于8KB  1024=1KB
  .priority = 10,
};
那么系统堆空间就还剩8kb，
然后任务中还有可能需要动态（malloc）申请堆内存
比如你的这个任务里面是执行LVGL的DOME，LVGL 的控件创建（lv_obj_create）
、资源加载（图标、字体）会通过 lv_mem_alloc 申请堆内存，
堆太小会导致控件创建失败，LVGL 卡死。

2.就算把RAM分配好了，使用了双缓存，帧率也没见提升
那是因为，你要驱动的800*480,如果使用双缓存需要更大的RAM空间
你的RAM内存支撑不住。
如果想要高刷，又不会用DMA，那就把LVGL的宽高减小，然后再使用双缓存模式
这样帧率就会上去了


这个版本加入了DMA2中断刷屏
效果：实测在优化等级2+DMA2中断刷屏+双缓存下，音乐dome动态最高11FPS CPU依旧95%


这个版本加入了外部SRAM存储LVGL双缓冲区，单个缓冲区大小为100kb
效果：减少占用单片机自带RAM内存，利于开辟新任务


这个版本更新信息：更新了UI，新增光照，气体检测。当光照低于一定阈值时，自动开灯；当检测有害气体超过阈值，会自动打开风扇，窗户排气并立刻发出警报，红灯亮起。当检测到有害气体低于阈值后会自动复位。（用步进电机或者舵机来代替开窗）。加入bootloaderAB分区，目前为串口IAP进行·固件升级，后续可升级为OTA升级，为系统进一步扩展功能，加入物联网平台中国移动oneoet。

踩坑经验：
1.首先应该在guiguider上设计好lvgl，然后中文选择SourceHanSerifSC_Regular.otf（思源宋体）
但是还是有问题，在guiguider仿真可以正常实现，但是移植到单片机上就会直接报错，

AI给出的原因是
Keil 的编译器（armcc）和编辑器对编码的处理是分开的，编辑器能识别 UTF-8，但编译器还是按 GBK/GB2312 去解析字符串，导致中文被截断。

解决：在代码中对应的字符串中加入空格间隔开两边

图标可以在阿里开源那里找，建议直接找好颜色的，不要自己改，不然会容易图片文件会变大


2.在添加弹窗的时候，需要先把弹窗标识勾选为隐藏，然后再按钮等等控件添加事件的时候，选择
显示，然后会有一个isxxx，勾选上就是显示，不勾选就是不显示

3.重点	
LVGL UI更新步骤：

如果是在原工程进行替换的话，首先把keil里面的mygui的相关c文件全部移除，除非是一些文件在更新前后都没有新增内容的，可以选择保留，头文件路径不用修改。

然后打开工程文件夹中的mygui文件夹，将原来的generated文件备份一份，然后把这个文件夹全部清空
再把导出到output文件  下的xxx工程中的generated文件     中的内容全部复制一份到keil工程那个
被清空的generated文件夹中。

最后回到keil，把图片c文件(screen_setting_LOG.c、screen_setting_exit_LOG.c)，

字体文件(比如lv_font_montserrat_14.c、lv_font_simsun_16.c)，

页面创建文件(setup_scr_screen.c、setup_scr_screen_setting.c)，	

以及重要文件(比如events_init.c、gui_guider.c、widgets_init.c)

都添加到mygui分支当中


然后就是编译并改错（新增的图片文件中，头文件导入时全部都是“lvgl/lvgl.h”要改为lvgl.h）



4.在写代码之前，一定要先把整体框架先设定好方案，比如说OTA整个流程怎么闭环起来，
物模型数据和OTA之间如何协调不影响，先做完app的业务功能，再去加入OTA流程，如果是先做完OTA，再做app的业务功能，那么调试起来就会很麻烦