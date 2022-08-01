#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>
#include  <Guid/FileInfo.h>

// #@@range_begin(struct_memory_map)
struct MemoryMap {
    UINTN buffer_size;
    VOID* buffer;
    UINTN map_size;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
};
// #@@range_end(struct_memory_map)

// #@@range_begin(get_memory_map)
EFI_STATUS GetMemoryMap(struct MemoryMap* map) {
    if (map->buffer == NULL) {
        return EFI_BUFFER_TOO_SMALL;
    }

    map->map_size = map->buffer_size;
    return gBS->GetMemoryMap(
        &(map->map_size),
        (EFI_MEMORY_DESCRIPTOR*)map->buffer,
        &(map->map_key),
        &(map->descriptor_size),
        &(map->descriptor_version)
    );
}
// #@@range_end(get_memory_map)

// #@@range_begin(get_memory_type)
const CHAR16* GetMemoryTypeUnicode(EFI_MEMORY_TYPE type) {
    switch (type) {
        case EfiReservedMemoryType: return L"EfiReservedMemoryType";
        case EfiLoaderCode: return L"EfiLoaderCode";
        case EfiLoaderData: return L"EfiLoaderData";
        case EfiBootServicesCode: return L"EfiBootServicesCode";
        case EfiBootServicesData: return L"EfiBootServicesData";
        case EfiRuntimeServicesCode: return L"EfiRuntimeServicesCode";
        case EfiRuntimeServicesData: return L"EfiRuntimeServicesData";
        case EfiConventionalMemory: return L"EfiConventionalMemory";
        case EfiUnusableMemory: return L"EfiUnusableMemory";
        case EfiACPIReclaimMemory: return L"EfiACPIReclaimMemory";
        case EfiACPIMemoryNVS: return L"EfiACPIMemoryNVS";
        case EfiMemoryMappedIO: return L"EfiMemoryMappedIO";
        case EfiMemoryMappedIOPortSpace: return L"EfiMemoryMappedIOPortSpace";
        case EfiPalCode: return L"EfiPalCode";
        case EfiPersistentMemory: return L"EfiPersistentMemory";
        case EfiMaxMemoryType: return L"EfiMaxMemoryType";
        default: return L"InvalidMemoryType";
    }
}
// #@@range_end(get_memory_type)

// #@@range_begin(save_memory_map)
EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file) {
    CHAR8 buf[256];
    UINTN len;

    CHAR8* header = "Index, Type(name, PhysicalStart, NumberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    file->Write(file, &len, header);

    Print(L"map->buffer = %08lx, map->map_size = %08lx\n", map->buffer, map->map_size);

    EFI_PHYSICAL_ADDRESS iter;
    int i;
    for (iter = (EFI_PHYSICAL_ADDRESS)map->buffer, i = 0;
            iter < (EFI_PHYSICAL_ADDRESS)map->buffer + map->map_size;
            iter += map->descriptor_size, ++i) {
        
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)iter;
        len = AsciiSPrint(buf, sizeof(buf), 
            "%u, %x, %-ls, %08lx, %lx, %lx\n", 
            i, desc->Type, GetMemoryTypeUnicode(desc->Type), 
            desc->PhysicalStart, desc->NumberOfPages, desc->Attribute & 0xffffflu);
        file->Write(file, &len, buf);
    }

    return EFI_SUCCESS;
}
// #@@range_end(save_memory_map)

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root) {
    EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;

    gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&loaded_image,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    gBS->OpenProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&fs,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    fs->OpenVolume(fs, root);

    return EFI_SUCCESS;
}

// #@@range_begin(main)
EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    // #@@ range_begin(write_memory_map_file)
    Print(L"Hello, Mikan World!\n");

    Print(L"Request memory map...");
    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
    GetMemoryMap(&memmap);
    Print(L"Done\n");

    EFI_FILE_PROTOCOL* root_dir;
    OpenRootDir(image_handle, &root_dir);

    Print(L"Opening \\memmap file...");
    EFI_FILE_PROTOCOL* memmap_file;
    root_dir->Open(
        root_dir, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0
    );
    Print(L"file opened\n");

    Print(L"Saving memory map into \\memmap...\n");
    SaveMemoryMap(&memmap, memmap_file);
    Print(L"Done\n");

    memmap_file->Close(memmap_file);
    // #@@ range_end(write_memory_map_file)

    // #@@ range_begin(read_kernel)
    Print(L"Read kernel from \\kernel.elf...\n");
    EFI_FILE_PROTOCOL* kernel_file;
    root_dir->Open(
        root_dir, &kernel_file, L"\\kernel.elf",
        EFI_FILE_MODE_READ, 0
    );

    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
    UINT8 file_info_buffer[file_info_size];
    kernel_file->GetInfo(
        kernel_file, &gEfiFileInfoGuid,
        &file_info_size, file_info_buffer
    );
    
    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*) file_info_buffer;
    UINTN kernel_file_size = file_info->FileSize;
    Print(L"file_name->%s, file_size->%lu\n", file_info->FileName, kernel_file_size);

    EFI_PHYSICAL_ADDRESS kernel_base_addr = 0x100000;
    gBS->AllocatePages(
        AllocateAddress, EfiLoaderData,
        (kernel_file_size + 0xfff) / 0x1000, &kernel_base_addr
    );
    kernel_file->Read(kernel_file, &kernel_file_size, (VOID*)kernel_base_addr);
    Print(L"kernel writed at 0x%0lx\n", kernel_base_addr);
    // #@@ range_end(read_kernel)

    // #@@ range_begin(exit_bs)
    EFI_STATUS status;
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    
    if (EFI_ERROR(status)) {
        status = GetMemoryMap(&memmap);
        if (EFI_ERROR(status)) {
            Print(L"failed to get memory map: %r\n", status);
            while (1);
        }
        status = gBS->ExitBootServices(image_handle, memmap.map_key);
        if (EFI_ERROR(status)) {
            Print(L"Could not exit boot ervices: %r\n", status);
            while (1);
        }
    }
    
    // #@@ range_end(exit_bs)

    // #@@ range_begin(call_kernel)
    UINT64 entry_addr = *(UINT64*)(kernel_base_addr + 24);

    typedef void EntryPointType(void);
    EntryPointType* entry_point = (EntryPointType*)entry_addr;
    entry_point();
    // #@@ range_end(call_kernel)

    while (1);
    return EFI_SUCCESS;
}
// #@@range_end(main)