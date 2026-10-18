/**
 * @file iap_flash_write_compare.c
 * @brief 早期IAP工程W25Q128分页写入错误与修复代码对比。
 *
 * @note 本文件只用于故障复盘，不加入当前Bootloader工程，也不参与编译。
 * @note 错误代码恢复自早期IAP工程testB1/Drivers/bootloader_drivers/src/TaskTest.c。
 */

/*
 * 原工程关键条件：
 * 1. XMODEM每包有效数据为128字节。
 * 2. 每收满8包，UpDataBuffer中共有1024字节。
 * 3. W25Q128一页为256字节，因此1024字节必须连续写4页。
 */

/**
 * @brief 对比“1024字节完整缓存”的错误写法和修复写法。
 * @param 无。
 * @return 无。
 * @note 依赖早期工程的全局变量，仅作为原代码摘录阅读。
 */
static void IAP_FullBufferWrite_Compare(void)
{
    uint16_t i;

    /*
     * 历史错误实现，禁止启用。
     * W25Q128_PAGE_SIZE等于256，所以下面的循环上限是1。
     * 结果：1024字节缓存只写入第1页，后面3页共768字节仍为0xFF。
     */
#if 0
    for (i = 0; i < (W25Q128_PAGE_SIZE / 256); i++)
    {
        W25Q128_PageWrite_ByPageNum(
            &updataA.UpDataBuffer[i * 256],
            (updataA.XmodemNum / 8 - 1) * 4 +
            i +
            updataA.W25Q128_BlockNum * 64 * 4);
    }
#endif

    /*
     * 修复实现：1024字节明确拆成4页连续写入。
     * 页号写成“块基址页号+组内页号+当前页偏移”，避免地址单位混乱。
     */
    for (i = 0; i < 4; i++)
    {
        uint16_t page_num =
            updataA.W25Q128_BlockNum * 256 +
            (updataA.XmodemNum / 8 - 1) * 4 +
            i;

        W25Q128_PageWrite_ByPageNum(
            &updataA.UpDataBuffer[i * 256],
            page_num);
    }
}
/**
 * @brief 对比“XMODEM尾包”的错误写法和修复写法。
 * @param 无。
 * @return 无。
 * @note 尾包可能为128~896字节，不能固定只写一页，也不能把无效填充值写入最后一页。
 */
static void IAP_TailBufferWrite_Compare(void)
{
    uint16_t i;

    /*
     * 历史错误实现，禁止启用。
     * 循环仍然只执行1次；当尾包超过256字节时，后续有效数据会丢失。
     * PageWrite_ByPageNum固定写整页，也无法表达最后一页的真实有效长度。
     */
#if 0
    for (i = 0; i < (W25Q128_PAGE_SIZE / 256); i++)
    {
        W25Q128_PageWrite_ByPageNum(
            &updataA.UpDataBuffer[i * 256],
            (updataA.XmodemNum / 8) * 4 +
            i +
            updataA.W25Q128_BlockNum * 64 * 4);
    }
#endif

    /* 修复实现：根据剩余有效字节数计算页数，并限制最后一页写入长度。 */
    {
        uint32_t remain_bytes = (updataA.XmodemNum % 8) * 128;
        uint32_t remain_pages = (remain_bytes + 255) / 256;

        for (i = 0; i < remain_pages; i++)
        {
            uint16_t page_num =
                updataA.W25Q128_BlockNum * 256 +
                (updataA.XmodemNum / 8) * 4 +
                i;
            uint32_t written_bytes = (uint32_t)i * 256;
            uint32_t write_len = remain_bytes - written_bytes;
            uint32_t page_addr = (uint32_t)page_num * W25Q128_PAGE_SIZE;

            if (write_len > 256)
            {
                write_len = 256;
            }

            W25Q128_PageWrite(
                page_addr,
                &updataA.UpDataBuffer[written_bytes],
                write_len);
        }
    }
}
