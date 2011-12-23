#include "qemu-common.h"
#include "cpu-common.h"
#include "targphys.h"
#include "rwhandler.h"
#include "hw/loader.h"
#include "hw/boards.h"

static uint32_t dummy_content[64];
const target_phys_addr_t dummy_address = 0xFFFF0000;

static uint32_t dummy_read(ReadWriteHandler *handler, pcibus_t address, int length)
{
    /* length is ignored.  */
    dummy_content[address] = dummy_content[address] ?: rand();
    printf("@%" PRIu64 " -> %" PRIu32 " (%d)\n", (uint64_t)address, dummy_content[address], length);
    return dummy_content[address];
}

static void dummy_write(ReadWriteHandler *handler, pcibus_t address, uint32_t value, int length)
{
    /* length is ignored.  */
    printf("@%" PRIu64 " <- %" PRIu32 " (%d)\n", (uint64_t)address, value, length);
    dummy_content[address] = value;
}

static ReadWriteHandler dummy_handler = {
    .write = dummy_write,
    .read  = dummy_read,
};

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
    cpu_state->pc = reset_address;

    /* Register the RAM at the reset address.  */
    status = qemu_ram_alloc(NULL, "dummy.ram", ram_size);
    cpu_register_physical_memory(reset_address, ram_size, status);

    /* Initialize the RAM with the content of the program image.  Be
     * careful, load_image() doesn't check if the image fits the
     * RAM.  */
    status = load_image(image_filename, qemu_get_ram_ptr(status));
    if (status < 0) {
        printf("Can't load '%s', aborting.\n", image_filename);
        exit(1);
    }

    /* Register the "dummy" handler. */
    status = cpu_register_io_memory_simple(&dummy_handler, DEVICE_NATIVE_ENDIAN);
    cpu_register_physical_memory(dummy_address, sizeof(dummy_content), status);
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
