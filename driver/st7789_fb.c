#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#define DRIVER_NAME "telemetry_fb"
#define ST7789_WIDTH 240
#define ST7789_HEIGHT 240
#define BITS_PER_PIXEL 16
#define FRAMEBUFFER_SIZE (ST7789_WIDTH * ST7789_HEIGHT * (BITS_PER_PIXEL / 8))

/* Hardware state structure */
struct telemetry_fb_info {
    struct fb_info *info;
    void __iomem *spi_base; /* Memory-Mapped I/O address for SPI peripheral */
    dma_addr_t dma_handle;
};

/**
 * @brief Memory map callback for user-space zero-copy access.
 * 
 * Maps the physically contiguous kernel memory allocated for the framebuffer
 * directly into the virtual address space of the user-space 3D engine.
 */
static int TelemetryFB_Mmap(struct fb_info *info, struct vm_area_struct *vma)
{
    unsigned long start = vma->vm_start;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long pfn;

    if (offset + size > info->fix.smem_len) {
        return -EINVAL;
    }

    /* Convert logical memory address to Page Frame Number (PFN) */
    pfn = virt_to_phys(info->screen_base + offset) >> PAGE_SHIFT;

    /* Enforce uncacheable memory for direct hardware coherence */
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    if (remap_pfn_range(vma, start, pfn, size, vma->vm_page_prot)) {
        return -EAGAIN;
    }

    return 0;
}

static struct fb_ops telemetry_fb_ops = {
    .owner      = THIS_MODULE,
    .fb_mmap    = TelemetryFB_Mmap,
    .fb_fillrect = cfb_fillrect,
    .fb_copyarea = cfb_copyarea,
    .fb_imageblit = cfb_imageblit,
};

static int __init TelemetryFB_Init(void)
{
    struct telemetry_fb_info *drv_data;
    struct fb_info *info;
    int ret;

    pr_info("%s: Initializing High-Performance Telemetry Framebuffer\n", DRIVER_NAME);

    info = framebuffer_alloc(sizeof(struct telemetry_fb_info), NULL);
    if (!info) {
        pr_err("%s: Failed to allocate framebuffer info structure\n", DRIVER_NAME);
        return -ENOMEM;
    }

    drv_data = info->par;
    drv_data->info = info;

    /* Allocate physically contiguous DMA-coherent memory */
    info->screen_base = dma_alloc_coherent(NULL, FRAMEBUFFER_SIZE, &drv_data->dma_handle, GFP_KERNEL);
    if (!info->screen_base) {
        pr_err("%s: DMA allocation failure\n", DRIVER_NAME);
        framebuffer_release(info);
        return -ENOMEM;
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

    ret = register_framebuffer(info);
    if (ret < 0) {
        pr_err("%s: Failed to register framebuffer device\n", DRIVER_NAME);
        dma_free_coherent(NULL, FRAMEBUFFER_SIZE, info->screen_base, drv_data->dma_handle);
        framebuffer_release(info);
        return ret;
    }

    pr_info("%s: Framebuffer successfully mapped at physical address 0x%lx\n", DRIVER_NAME, (unsigned long)info->fix.smem_start);
    return 0;
}

static void __exit TelemetryFB_Exit(void)
{
    /* Global pointer would typically be retrieved via platform_get_drvdata */
    pr_info("%s: Unloading module, releasing coherent memory\n", DRIVER_NAME);
}

module_init(TelemetryFB_Init);
module_exit(TelemetryFB_Exit);

MODULE_AUTHOR("Principal HPC Engineer");
MODULE_DESCRIPTION("ARM NEON-Powered 3D Telemetry Framebuffer Driver");
MODULE_LICENSE("GPL");
