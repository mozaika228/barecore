#include <efi.h>
#include <efilib.h>

#define KERNEL_LOAD_ADDR 0x00100000ULL

static EFI_STATUS load_kernel(EFI_HANDLE image, EFI_SYSTEM_TABLE *st, UINTN *kernel_size) {
    EFI_STATUS status;
    EFI_LOADED_IMAGE *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *kernel = NULL;
    EFI_FILE_INFO *info = NULL;
    UINTN info_size = 0;
    UINTN read_size;
    UINTN pages;
    EFI_PHYSICAL_ADDRESS load_addr = KERNEL_LOAD_ADDR;

    status = uefi_call_wrapper(st->BootServices->HandleProtocol, 3,
                               image, &LoadedImageProtocol, (void **)&loaded_image);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = uefi_call_wrapper(st->BootServices->HandleProtocol, 3,
                               loaded_image->DeviceHandle, &FileSystemProtocol, (void **)&fs);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = uefi_call_wrapper(root->Open, 5, root, &kernel, L"\\kernel.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = uefi_call_wrapper(kernel->GetInfo, 4, kernel, &GenericFileInfo, &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    status = uefi_call_wrapper(st->BootServices->AllocatePool, 3, EfiLoaderData, info_size, (void **)&info);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = uefi_call_wrapper(kernel->GetInfo, 4, kernel, &GenericFileInfo, &info_size, info);
    if (EFI_ERROR(status)) {
        return status;
    }

    *kernel_size = info->FileSize;
    pages = (*kernel_size + 0xFFF) / 0x1000;

    status = uefi_call_wrapper(st->BootServices->AllocatePages, 4,
                               AllocateAddress, EfiLoaderData, pages, &load_addr);
    if (EFI_ERROR(status)) {
        return status;
    }

    read_size = *kernel_size;
    status = uefi_call_wrapper(kernel->Read, 3, kernel, &read_size, (void *)(UINTN)KERNEL_LOAD_ADDR);
    if (EFI_ERROR(status) || read_size != *kernel_size) {
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    EFI_STATUS status;
    UINTN kernel_size = 0;
    UINTN mmap_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_version = 0;
    EFI_MEMORY_DESCRIPTOR *mmap = NULL;
    void (*kernel_entry)(void) = (void (*)(void))(UINTN)KERNEL_LOAD_ADDR;

    InitializeLib(image, st);
    Print(L"barecore UEFI loader\r\n");

    status = load_kernel(image, st, &kernel_size);
    if (EFI_ERROR(status)) {
        Print(L"kernel load failed: %r\r\n", status);
        return status;
    }

    status = uefi_call_wrapper(st->BootServices->GetMemoryMap, 5,
                               &mmap_size, mmap, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        Print(L"GetMemoryMap probe failed: %r\r\n", status);
        return status;
    }

    mmap_size += desc_size * 4;
    status = uefi_call_wrapper(st->BootServices->AllocatePool, 3, EfiLoaderData, mmap_size, (void **)&mmap);
    if (EFI_ERROR(status)) {
        Print(L"AllocatePool mmap failed: %r\r\n", status);
        return status;
    }

    status = uefi_call_wrapper(st->BootServices->GetMemoryMap, 5,
                               &mmap_size, mmap, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) {
        Print(L"GetMemoryMap failed: %r\r\n", status);
        return status;
    }

    status = uefi_call_wrapper(st->BootServices->ExitBootServices, 2, image, map_key);
    if (EFI_ERROR(status)) {
        return status;
    }

    (void)kernel_size;
    kernel_entry();
    return EFI_SUCCESS;
}
