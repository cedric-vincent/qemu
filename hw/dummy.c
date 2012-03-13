#include "qemu-common.h"
#include "cpu-common.h"
#include "targphys.h"
#include "memory.h"
#include "exec-memory.h"
#include "hw/loader.h"
#include "hw/boards.h"

static uint64_t dummy_content[64];
const target_phys_addr_t dummy_address = 0xFFFF0000;

static uint64_t dummy_read(void *opaque, target_phys_addr_t address, unsigned size)
{
    dummy_content[address] = dummy_content[address] ?: rand();
    printf("@%" PRIu64 " -> %" PRIu64 " (%u)\n", (uint64_t)address, dummy_content[address], size);
    return dummy_content[address];
}

static void dummy_write(void *opaque, target_phys_addr_t address, uint64_t value, unsigned size)
{
    printf("@%" PRIu64 " <- %" PRIu64 " (%u)\n", (uint64_t)address, value, size);
    dummy_content[address] = value;
}

static const MemoryRegionOps dummy_handler = {
    .read = dummy_read,
    .write = dummy_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    }
};

static MemoryRegion dummy_iomem;
static MemoryRegion dummy_vram;

static void dummy_init(ram_addr_t ram_size, const char *boot_device,
                       const char *kernel_filename, const char *kernel_cmdline,
                       const char *initrd_filename, const char *cpu_model)
{
    const target_phys_addr_t reset_address = 0x00000000;
    const char *image_filename = "dummy.bin";
    CPUState *cpu_state;
    int status;

    if (!cpu_model)
        cpu_model = "any";

    /* Initialize the CPU and for the reset address.  */
    cpu_state = cpu_init(cpu_model);

#ifdef TARGET_ARM
#  define PC regs[15]
#else
#  define PC pc
#endif
    cpu_state->PC = reset_address;

    /* Register the RAM at the reset address.  */
    memory_region_init_ram(&dummy_vram, NULL, "dummy.vram", ram_size);
    memory_region_add_subregion(get_system_memory(), reset_address, &dummy_vram);

    /* Initialize the RAM with the content of the program image.  Be
     * careful, load_image() doesn't check if the image fits the
     * RAM.  */
    status = load_image(image_filename, memory_region_get_ram_ptr(&dummy_vram));
    if (status < 0) {
        printf("Can't load '%s', aborting.\n", image_filename);
        exit(1);
    }

    /* Register the "dummy" handler. */
    memory_region_init_io(&dummy_iomem, &dummy_handler, NULL, "dummy.iomem", sizeof(dummy_content));
    memory_region_add_subregion(get_system_memory(), dummy_address, &dummy_iomem);
}

static QEMUMachine dummy_machine = {
    .name = "dummy",
    .desc = "dummy card",
    .init = dummy_init,
    .is_default = 0,
};

static void dummy_machine_init(void)
{
    qemu_register_machine(&dummy_machine);
}

machine_init(dummy_machine_init);
