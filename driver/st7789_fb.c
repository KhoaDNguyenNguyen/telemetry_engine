#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include "../core/inc/st7789_logic.h"
#include "hal_spi_rpi.h"

#define DRIVER_NAME "telemetry_fb"
#define ST7789_WIDTH 240
#define ST7789_HEIGHT 240
#define BITS_PER_PIXEL 16
#define FRAMEBUFFER_SIZE (ST7789_WIDTH * ST7789_HEIGHT * (BITS_PER_PIXEL / 8))

struct telemetry_fb_info {
    struct fb_info *info;
    dma_addr_t dma_handle;
    const spi_driver_ops_t *spi_ops;
    struct fb_deferred_io defio;
};

static void TelemetryFB_Deferred_IO(struct fb_info *info, struct list_head *pagelist)
{
    struct telemetry_fb_info *drv_data = info->par;
    
    /* Hardware projection of the virtual framebuffer to the SPI subsystem */
    if (drv_data->spi_ops) {
        ST7789_FlushBuffer(drv_data->spi_ops, (uint16_t *)info->screen_base, ST7789_WIDTH * ST7789_HEIGHT);
    }
}

static int TelemetryFB_Mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long pfn;

    if (offset + size > info->fix.smem_len) {
        return -EINVAL;
    }

    pfn = virt_to_phys(info->screen_base + offset) >> PAGE_SHIFT;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    if (remap_pfn_range(vma, start, pfn, size, vma->vm_page_prot)) {
        return -EAGAIN;
    }

    return 0;
}

static struct fb_ops telemetry_fb_ops = {
    .owner       = THIS_MODULE,
    .fb_mmap     = TelemetryFB_Mmap,
    .fb_fillrect = sys_fillrect,
    .fb_copyarea = sys_copyarea,
    .fb_imageblit= sys_imageblit,
};

static int TelemetryFB_Probe(struct platform_device *pdev)
{
    struct telemetry_fb_info *drv_data;
    struct fb_info *info;
    int ret;

    pr_info("%s: Probing Device Tree for ST7789 interface.\n", DRIVER_NAME);

    info = framebuffer_alloc(sizeof(struct telemetry_fb_info), &pdev->dev);
    if (!info) return -ENOMEM;

    drv_data = info->par;
    drv_data->info = info;

    /* Initialize Hardware Abstraction Layer */
    drv_data->spi_ops = RPi_SPI_Init(pdev);
    if (!drv_data->spi_ops) {
        ret = -EIO;
        goto err_fb_alloc;
    }

    /* Execute Hardware Initialization Sequence */
    ST7789_InitSequence(drv_data->spi_ops);

    info->screen_base = dma_alloc_coherent(&pdev->dev, FRAMEBUFFER_SIZE, &drv_data->dma_handle, GFP_KERNEL);
    if (!info->screen_base) {
        ret = -ENOMEM;
        goto err_spi_init;
    }

    info->screen_size = FRAMEBUFFER_SIZE;
    info->fix.smem_start = drv_data->dma_handle;
    info->fix.smem_len = FRAMEBUFFER_SIZE;
    info->fix.type = FB_TYPE_PACKED_PIXELS;
    info->fix.visual = FB_VISUAL_TRUECOLOR;
    info->fix.line_length = ST7789_WIDTH * (BITS_PER_PIXEL / 8);

    info->var.xres = ST7789_WIDTH;
    info->var.yres = ST7789_HEIGHT;
    info->var.xres_virtual = ST7789_WIDTH;
    info->var.yres_virtual = ST7789_HEIGHT;
    info->var.bits_per_pixel = BITS_PER_PIXEL;
    info->fbops = &telemetry_fb_ops;

    /* Initialize Deferred I/O Polling (Set for 60 FPS equivalent) */
    drv_data->defio.delay = HZ / 60;
    drv_data->defio.deferred_io = TelemetryFB_Deferred_IO;
    info->fbdefio = &drv_data->defio;
    fb_deferred_io_init(info);

    ret = register_framebuffer(info);
    if (ret < 0) goto err_dma_alloc;

    platform_set_drvdata(pdev, drv_data);
    pr_info("%s: DMA Framebuffer mapped and Deferred I/O established.\n", DRIVER_NAME);
    return 0;

err_dma_alloc:
    fb_deferred_io_cleanup(info);
    dma_free_coherent(&pdev->dev, FRAMEBUFFER_SIZE, info->screen_base, drv_data->dma_handle);
err_spi_init:
    RPi_SPI_Deinit();
err_fb_alloc:
    framebuffer_release(info);
    return ret;
}

static int TelemetryFB_Remove(struct platform_device *pdev)
{
    struct telemetry_fb_info *drv_data = platform_get_drvdata(pdev);
    
    unregister_framebuffer(drv_data->info);
    fb_deferred_io_cleanup(drv_data->info);
    dma_free_coherent(&pdev->dev, FRAMEBUFFER_SIZE, drv_data->info->screen_base, drv_data->dma_handle);
    RPi_SPI_Deinit();
    framebuffer_release(drv_data->info);
    
    return 0;
}

static const struct of_device_id telemetry_fb_of_match[] = {
    { .compatible = "telemetry,st7789", },
    { },
};
MODULE_DEVICE_TABLE(of, telemetry_fb_of_match);

static struct platform_driver telemetry_fb_driver = {
    .probe  = TelemetryFB_Probe,
    .remove = TelemetryFB_Remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = telemetry_fb_of_match,
    },
};

module_platform_driver(telemetry_fb_driver);

MODULE_AUTHOR("Principal HPC Engineer");
MODULE_DESCRIPTION("Deferred I/O Framebuffer mapping to MMIO SPI");
MODULE_LICENSE("GPL");
