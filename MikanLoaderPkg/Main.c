#include  <Uefi.h>
#include  <Library/UefiLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/PrintLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Library/BaseMemoryLib.h>
#include  <Protocol/LoadedImage.h>
#include  <Protocol/SimpleFileSystem.h>
#include  <Protocol/DiskIo2.h>
#include  <Protocol/BlockIo.h>
#include  <Guid/FileInfo.h>

#include "frame_buffer_config.hpp"
#include "memory_map.hpp"
#include "elf.hpp"


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

EFI_STATUS SaveMemoryMap(struct MemoryMap* map, EFI_FILE_PROTOCOL* file) {
    CHAR8 buf[256];
    UINTN len;
    EFI_STATUS status;

    CHAR8* header = "Index, Type(name, PhysicalStart, NumberOfPages, Attribute\n";
    len = AsciiStrLen(header);
    status = file->Write(file, &len, header);
    if (EFI_ERROR(status)) {
        return status;
    }

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
        status = file->Write(file, &len, buf);
        if (EFI_ERROR(status)) {
            return status;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS OpenRootDir(EFI_HANDLE image_handle, EFI_FILE_PROTOCOL** root) {
    EFI_LOADED_IMAGE_PROTOCOL* loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
    EFI_STATUS status;

    status = gBS->OpenProtocol(
        image_handle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&loaded_image,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );
    if (EFI_ERROR(status)) {
        return status;
    }

    status = gBS->OpenProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&fs,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );
    if (EFI_ERROR(status)) {
        return status;
    }

    return fs->OpenVolume(fs, root);
}

EFI_STATUS OpenGOP(EFI_HANDLE image_handle, EFI_GRAPHICS_OUTPUT_PROTOCOL** gop) {
    UINTN num_gop_handles = 0;
    EFI_HANDLE* gop_handles = NULL;
    EFI_STATUS status;

    status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        &num_gop_handles,
        &gop_handles
    );
    if (EFI_ERROR(status)) {
        return status;
    }

    status = gBS->OpenProtocol(
        gop_handles[0],
        &gEfiGraphicsOutputProtocolGuid,
        (VOID**)gop,
        image_handle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );
    if (EFI_ERROR(status)) {
        return status;
    }

    FreePool(gop_handles);
    
    return EFI_SUCCESS;
}

const CHAR16* GetPixelFormatUnicode(EFI_GRAPHICS_PIXEL_FORMAT fmt) {
    switch (fmt) {
        case PixelRedGreenBlueReserved8BitPerColor:
            return L"PixelRedGreenBlueReserved8BitPerColor";
        case PixelBlueGreenRedReserved8BitPerColor:
            return L"PixelBlueGreenRedReserved8BitPerColor";
        case PixelBitMask:
            return L"PixelBitMask";
        case PixelBltOnly:
            return L"PixelBltOnly";
        case PixelFormatMax:
            return L"PixelFormatMax";
        default:
            return L"InvalidPixelFormat";
    }
}

void CalcLoadAddressRange(Elf64_Ehdr* ehdr, UINT64* first, UINT64* last) {
    Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
    *first = MAX_INT64;
    *last = 0;
    for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type != PT_LOAD) continue;
        *first = MIN(*first, phdr[i].p_vaddr);
        *last = MAX(*last, phdr[i].p_vaddr + phdr[i].p_memsz);
    }
}

void CopyLoadSegments(Elf64_Ehdr* ehdr) {
    Elf64_Phdr* phdr = (Elf64_Phdr*)((UINT64)ehdr + ehdr->e_phoff);
    for (Elf64_Half i = 0; i < ehdr->e_phnum; ++i) {
        if (phdr[i].p_type != PT_LOAD) continue;

        UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset;
        CopyMem((VOID*)phdr[i].p_vaddr, (VOID*)segm_in_file, phdr[i].p_filesz);

        UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
        SetMem((VOID*)(phdr[i].p_vaddr + phdr[i].p_filesz), remain_bytes, 0);
    }
}

void Halt(void) {
    while (1) __asm__("hlt");
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    EFI_STATUS status;
    // #@@ range_begin(write_memory_map_file)
    Print(L"Hello, Mikan World!\n");

    Print(L"Request memory map...");
    CHAR8 memmap_buf[4096 * 4];
    struct MemoryMap memmap = {sizeof(memmap_buf), memmap_buf, 0, 0, 0, 0};
    status = GetMemoryMap(&memmap);
    if (EFI_ERROR(status)) {
        Print(L"Failed to get memory map: %r\n", status);
        Halt();
    }
    Print(L"Done\n");

    EFI_FILE_PROTOCOL* root_dir;
    status = OpenRootDir(image_handle, &root_dir);
    if (EFI_ERROR(status)) {
        Print(L"Failed to open root directory: %r\n", status);
        Halt();
    }

    Print(L"Opening \\memmap file...");
    EFI_FILE_PROTOCOL* memmap_file;
    status = root_dir->Open(
        root_dir, &memmap_file, L"\\memmap",
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0
    );
    if (EFI_ERROR(status)) {
        Print(L"Failed to open file \\memmap: %r\n", status);
    }
    else {
        Print(L"file opened\n");
        Print(L"Saving memory map into \\memmap...\n");
        status = SaveMemoryMap(&memmap, memmap_file);
        if (EFI_ERROR(status)) {
            Print(L"Failed to save memory map to \\memmap: %r\n", status);\
        } else {
            Print(L"Done\n");
        }
        memmap_file->Close(memmap_file);
    }
    // #@@ range_end(write_memory_map_file)

    // #@@ range_begin(gop)
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    status = OpenGOP(image_handle, &gop);
    if (EFI_ERROR(status)) {
        Print(L"Failed to open GOP: %r\n", status);
        Halt();
    }

    Print(
        L"Resolution: %ux%u, Pixel Format: %s, %u pixels/line\n",
        gop->Mode->Info->HorizontalResolution,
        gop->Mode->Info->VerticalResolution,
        GetPixelFormatUnicode(gop->Mode->Info->PixelFormat),
        gop->Mode->Info->PixelsPerScanLine
    );
    Print(
        L"Frame Buffer: 0x%0lx - 0x%0lx, Size: %lu bytes\n",
        gop->Mode->FrameBufferBase,
        gop->Mode->FrameBufferBase + gop->Mode->FrameBufferSize,
        gop->Mode->FrameBufferSize
    );
    UINT8* frame_buffer = (UINT8*)gop->Mode->FrameBufferBase;
    for (UINTN i = 0; i < gop->Mode->FrameBufferSize - 1024; i+=4) {
        frame_buffer[i] = 255;
    }
    // #@@ range_end(gop)

    // #@@ range_begin(read_kernel)
    Print(L"Read kernel from \\kernel.elf...\n");
    EFI_FILE_PROTOCOL* kernel_file;
    status = root_dir->Open(
        root_dir, &kernel_file, L"\\kernel.elf",
        EFI_FILE_MODE_READ, 0
    );
    if (EFI_ERROR(status)) {
        Print(L"Failed to open kernel on file \\kernel.elf: %r\n", status);
        Halt();
    }

    UINTN file_info_size = sizeof(EFI_FILE_INFO) + sizeof(CHAR16) * 12;
    UINT8 file_info_buffer[file_info_size];
    status = kernel_file->GetInfo(
        kernel_file, &gEfiFileInfoGuid,
        &file_info_size, file_info_buffer
    );
    if (EFI_ERROR(status)) {
        Print(L"Failed to get file info \\kernel.elf: %r\n", status);
        Halt();
    }
    
    EFI_FILE_INFO* file_info = (EFI_FILE_INFO*) file_info_buffer;
    UINTN kernel_file_size = file_info->FileSize;
    Print(L"file_name->%s, file_size->%lu\n", file_info->FileName, kernel_file_size);

    VOID* kernel_buffer;
    status = gBS->AllocatePool(EfiLoaderData, kernel_file_size, &kernel_buffer);
    if (EFI_ERROR(status)) {
        Print(L"Failed to allocate poll: %r\n", status);
        Halt();
    }

    status = kernel_file->Read(kernel_file, &kernel_file_size, kernel_buffer);
    if (EFI_ERROR(status)) {
        Print(L"Failed to load kernel to temporary buffer: %r\n", status);
        Halt();
    }

    Elf64_Ehdr* kernel_ehdr = (Elf64_Ehdr*)kernel_buffer;
    UINT64 kernel_first_addr, kernel_last_addr;
    CalcLoadAddressRange(kernel_ehdr, &kernel_first_addr, &kernel_last_addr);

    UINTN num_pages = (kernel_last_addr - kernel_first_addr + 0xfff) / 0x1000;
    status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, num_pages, &kernel_first_addr);
    if (EFI_ERROR(status)) {
        Print(L"Failed to allocate pages for kernel from 0x%0lx: %r\n", kernel_first_addr, status);
        Halt();
    }

    CopyLoadSegments(kernel_ehdr);
    Print(L"Kernel loaded: 0x%0lx - 0x%0lx\n", kernel_first_addr, kernel_last_addr);

    status = gBS->FreePool(kernel_buffer);
    if (EFI_ERROR(status)) {
        Print(L"failed to free pool: %r\n", status);
        Halt();
    }
    // #@@ range_end(read_kernel)

    // #@@ range_begin(pass_frame_buffer_config)
    struct FrameBufferConfig fb_config = {
        (UINT8*)gop->Mode->FrameBufferBase,
        gop->Mode->Info->PixelsPerScanLine,
        gop->Mode->Info->HorizontalResolution,
        gop->Mode->Info->VerticalResolution,
        0
    };

    switch (gop->Mode->Info->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor:
            fb_config.pixel_format = kPixelRGBResv8BitPerColor;
            break;
        case PixelBlueGreenRedReserved8BitPerColor:
            fb_config.pixel_format = kPixelBGRResv8BitPerColor;
            break;
        default:
            Print(L"Unimplemented pixel format: %d\n", gop->Mode->Info->PixelFormat);
            Halt();
    }
    // #@@ range_begin(pass_frame_buffer_config)

    // #@@ range_begin(exit_bs)
    status = gBS->ExitBootServices(image_handle, memmap.map_key);
    
    if (EFI_ERROR(status)) {
        status = GetMemoryMap(&memmap);
        if (EFI_ERROR(status)) {
            Print(L"failed to get memory map: %r\n", status);
            Halt();
        }
        status = gBS->ExitBootServices(image_handle, memmap.map_key);
        if (EFI_ERROR(status)) {
            Print(L"Could not exit boot ervices: %r\n", status);
            Halt();
        }
    }
    // #@@ range_end(exit_bs)
    
    // #@@ range_begin(call_kernel)
    UINT64 entry_addr = *(UINT64*)(kernel_first_addr + 24);

    typedef void EntryPointType(const struct FrameBufferConfig*, const struct MemoryMap*);
    EntryPointType* entry_point = (EntryPointType*)entry_addr;
    entry_point(&fb_config, &memmap);
    // #@@ range_end(call_kernel)

    Halt();
    return EFI_SUCCESS;
}
// #@@range_end(main)