#include <ntifs.h>
#include <ntimage.h>
#include <ntstrsafe.h>

NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(ULONG SystemInformationClass,
                                                 PVOID SystemInformation,
                                                 ULONG SystemInformationLength,
                                                 PULONG ReturnLength);

NTKERNELAPI VOID KeStackAttachProcess(PEPROCESS Process, PKAPC_STATE ApcState);
NTKERNELAPI VOID KeUnstackDetachProcess(PKAPC_STATE ApcState);

typedef struct _MY_SYSTEM_PROCESS_INFO {
  ULONG NextEntryOffset;
  ULONG NumberOfThreads;
  LARGE_INTEGER WorkingSetPrivateSize;
  ULONG HardFaultCount;
  ULONG NumberOfThreadsHighWatermark;
  ULONGLONG CycleTime;
  LARGE_INTEGER CreateTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER KernelTime;
  UNICODE_STRING ImageName;
  LONG BasePriority;
  HANDLE UniqueProcessId;
  HANDLE InheritedFromUniqueProcessId;
  ULONG HandleCount;
  ULONG SessionId;
  ULONG_PTR UniqueProcessKey;
  SIZE_T PeakVirtualSize;
  SIZE_T VirtualSize;
  ULONG PageFaultCount;
  SIZE_T PeakWorkingSetSize;
  SIZE_T WorkingSetSize;
  SIZE_T QuotaPeakPagedPoolUsage;
  SIZE_T QuotaPagedPoolUsage;
  SIZE_T QuotaPeakNonPagedPoolUsage;
  SIZE_T QuotaNonPagedPoolUsage;
  SIZE_T PagefileUsage;
  SIZE_T PeakPagefileUsage;
  SIZE_T PrivatePageCount;
  LARGE_INTEGER ReadOperationCount;
  LARGE_INTEGER WriteOperationCount;
  LARGE_INTEGER OtherOperationCount;
  LARGE_INTEGER ReadTransferCount;
  LARGE_INTEGER WriteTransferCount;
  LARGE_INTEGER OtherTransferCount;
} MY_SYSTEM_PROCESS_INFO, *PMY_SYSTEM_PROCESS_INFO;

// =====================================================
// IOCTL Codes - must match ext/memory.h
// =====================================================
#define NSI_DEVICE 0x8500
#define IOCTL_ATTACH                                                           \
  CTL_CODE(NSI_DEVICE, 0x4752, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_READ                                                             \
  CTL_CODE(NSI_DEVICE, 0x4753, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_GET_MODULE_BASE                                                  \
  CTL_CODE(NSI_DEVICE, 0x4754, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_GET_PID                                                          \
  CTL_CODE(NSI_DEVICE, 0x4755, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_WRITE                                                            \
  CTL_CODE(NSI_DEVICE, 0x4756, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

typedef struct _REQUEST {
  HANDLE process_id;
  PVOID target;
  PVOID buffer;
  SIZE_T size;
} REQUEST, *PREQUEST;

typedef struct _PID_PACK {
  UINT32 pid;
  WCHAR name[1024];
} PID_PACK, *PPID_PACK;

typedef struct _MODULE_PACK {
  UINT32 pid;
  UINT64 baseAddress;
  SIZE_T size;
  WCHAR moduleName[1024];
} MODULE_PACK, *PMODULE_PACK;

typedef struct _WRITE_REQUEST {
  PVOID target;
  SIZE_T size;
  UCHAR data[256];
} WRITE_REQUEST, *PWRITE_REQUEST;

NTKERNELAPI NTSTATUS NTAPI MmCopyVirtualMemory(
    PEPROCESS SourceProcess, PVOID SourceAddress, PEPROCESS TargetProcess,
    PVOID TargetAddress, SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode,
    PSIZE_T ReturnSize);

NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName,
                                    PDRIVER_INITIALIZE InitializationFunction);

NTKERNELAPI PPEB PsGetProcessPeb(PEPROCESS Process);
typedef struct _PEB_LDR_DATA_FULL {
  ULONG Length;
  BOOLEAN Initialized;
  HANDLE SsHandle;
  LIST_ENTRY InLoadOrderModuleList;
  LIST_ENTRY InMemoryOrderModuleList;
  LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA_FULL, *PPEB_LDR_DATA_FULL;

typedef struct _LDR_DATA_TABLE_ENTRY_FULL {
  LIST_ENTRY InLoadOrderLinks;
  LIST_ENTRY InMemoryOrderLinks;
  LIST_ENTRY InInitializationOrderLinks;
  PVOID DllBase;
  PVOID EntryPoint;
  ULONG SizeOfImage;
  UNICODE_STRING FullDllName;
  UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY_FULL, *PLDR_DATA_TABLE_ENTRY_FULL;

typedef struct _PEB_FULL {
  UCHAR Reserved1[2];
  UCHAR BeingDebugged;
  UCHAR Reserved2[1];
  PVOID Reserved3[2];
  PPEB_LDR_DATA_FULL Ldr;
} PEB_FULL, *PPEB_FULL;

static PDEVICE_OBJECT g_DeviceObject = NULL;
static PEPROCESS g_TargetProcess = NULL;

static NTSTATUS KernelReadMemory(PEPROCESS Process, PVOID Source, PVOID Dest,
                                 SIZE_T Size) {
  SIZE_T bytes = 0;
  if (!Process || !Source || !Dest || Size == 0)
    return STATUS_INVALID_PARAMETER;

  return MmCopyVirtualMemory(Process, Source, PsGetCurrentProcess(), Dest, Size,
                             KernelMode, &bytes);
}

static UINT32 FindProcessIdByName(const WCHAR *processName) {
  ULONG bufferSize = 1024 * 1024;
  PVOID buffer = ExAllocatePoolWithTag(NonPagedPool, bufferSize, 'dPiF');
  if (!buffer)
    return 0;

  NTSTATUS status = ZwQuerySystemInformation(5 /* SystemProcessInformation */,
                                             buffer, bufferSize, &bufferSize);
  if (!NT_SUCCESS(status)) {
    ExFreePoolWithTag(buffer, 'dPiF');
    buffer = ExAllocatePoolWithTag(NonPagedPool, bufferSize, 'dPiF');
    if (!buffer)
      return 0;
    status = ZwQuerySystemInformation(5, buffer, bufferSize, &bufferSize);
    if (!NT_SUCCESS(status)) {
      ExFreePoolWithTag(buffer, 'dPiF');
      return 0;
    }
  }

  UINT32 foundPid = 0;
  PMY_SYSTEM_PROCESS_INFO procInfo = (PMY_SYSTEM_PROCESS_INFO)buffer;

  while (TRUE) {
    if (procInfo->ImageName.Buffer && procInfo->ImageName.Length > 0) {
      if (_wcsicmp(procInfo->ImageName.Buffer, processName) == 0) {
        foundPid = (UINT32)(ULONG_PTR)procInfo->UniqueProcessId;
        break;
      }
    }

    if (procInfo->NextEntryOffset == 0)
      break;
    procInfo =
        (PMY_SYSTEM_PROCESS_INFO)((PUCHAR)procInfo + procInfo->NextEntryOffset);
  }

  ExFreePoolWithTag(buffer, 'dPiF');
  return foundPid;
}

static UINT64 FindModuleBase(UINT32 pid, const WCHAR *moduleName) {
  PEPROCESS process = NULL;
  NTSTATUS status = PsLookupProcessByProcessId(ULongToHandle(pid), &process);
  if (!NT_SUCCESS(status))
    return 0;

  PPEB peb = PsGetProcessPeb(process);
  if (!peb) {
    ObDereferenceObject(process);
    return 0;
  }

  KAPC_STATE apcState;
  KeStackAttachProcess(process, &apcState);

  UINT64 result = 0;

  __try {
    PEB_FULL pebData = {0};
    RtlCopyMemory(&pebData, peb, sizeof(PEB_FULL));

    if (pebData.Ldr) {
      PEB_LDR_DATA_FULL ldrData = {0};
      RtlCopyMemory(&ldrData, pebData.Ldr, sizeof(PEB_LDR_DATA_FULL));

      LIST_ENTRY *head = &pebData.Ldr->InLoadOrderModuleList;
      LIST_ENTRY *current = ldrData.InLoadOrderModuleList.Flink;

      while (current != head) {
        LDR_DATA_TABLE_ENTRY_FULL entry = {0};
        RtlCopyMemory(&entry,
                      CONTAINING_RECORD(current, LDR_DATA_TABLE_ENTRY_FULL,
                                        InLoadOrderLinks),
                      sizeof(LDR_DATA_TABLE_ENTRY_FULL));

        if (entry.BaseDllName.Buffer && entry.BaseDllName.Length > 0 &&
            entry.BaseDllName.Length < 512) {
          WCHAR nameBuffer[256] = {0};
          RtlCopyMemory(nameBuffer, entry.BaseDllName.Buffer,
                        min(entry.BaseDllName.Length, sizeof(nameBuffer) - 2));

          if (_wcsicmp(nameBuffer, moduleName) == 0) {
            result = (UINT64)entry.DllBase;
            break;
          }
        }

        current = entry.InLoadOrderLinks.Flink;
        if (!current)
          break;
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    result = 0;
  }

  KeUnstackDetachProcess(&apcState);
  ObDereferenceObject(process);
  return result;
}

static NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  UNREFERENCED_PARAMETER(DeviceObject);
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

static NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  UNREFERENCED_PARAMETER(DeviceObject);

  PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
  NTSTATUS status = STATUS_SUCCESS;
  ULONG bytesReturned = 0;

  PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
  ULONG inputLength = stack->Parameters.DeviceIoControl.InputBufferLength;
  ULONG outputLength = stack->Parameters.DeviceIoControl.OutputBufferLength;

  switch (stack->Parameters.DeviceIoControl.IoControlCode) {
  case IOCTL_ATTACH: {
    if (inputLength < sizeof(REQUEST)) {
      status = STATUS_BUFFER_TOO_SMALL;
      break;
    }

    PREQUEST req = (PREQUEST)inputBuffer;
    PEPROCESS process = NULL;
    status = PsLookupProcessByProcessId(req->process_id, &process);
    if (NT_SUCCESS(status)) {
      if (g_TargetProcess)
        ObDereferenceObject(g_TargetProcess);
      g_TargetProcess = process;
    }
    bytesReturned = sizeof(REQUEST);
    break;
  }

  case IOCTL_READ: {
    if (inputLength < sizeof(REQUEST)) {
      status = STATUS_BUFFER_TOO_SMALL;
      break;
    }

    PREQUEST req = (PREQUEST)inputBuffer;

    if (!g_TargetProcess) {
      PEPROCESS process = NULL;
      status = PsLookupProcessByProcessId(req->process_id, &process);
      if (NT_SUCCESS(status)) {
        if (g_TargetProcess)
          ObDereferenceObject(g_TargetProcess);
        g_TargetProcess = process;
      } else {
        status = STATUS_INVALID_PARAMETER;
        break;
      }
    }

    if (req->target && req->buffer && req->size > 0 && req->size <= 0x10000) {
      SIZE_T bytes = 0;
      status = MmCopyVirtualMemory(g_TargetProcess, req->target,
                                   PsGetCurrentProcess(), req->buffer,
                                   req->size, KernelMode, &bytes);
    } else {
      status = STATUS_INVALID_PARAMETER;
    }
    bytesReturned = sizeof(REQUEST);
    break;
  }

  case IOCTL_WRITE: {
    if (inputLength < sizeof(WRITE_REQUEST)) {
      status = STATUS_BUFFER_TOO_SMALL;
      break;
    }

    PWRITE_REQUEST req = (PWRITE_REQUEST)inputBuffer;

    if (!g_TargetProcess) {
      status = STATUS_INVALID_PARAMETER;
      break;
    }

    if (req->target && req->size > 0 && req->size <= 256) {
      SIZE_T bytes = 0;
      status =
          MmCopyVirtualMemory(PsGetCurrentProcess(), req->data, g_TargetProcess,
                              req->target, req->size, KernelMode, &bytes);
    } else {
      status = STATUS_INVALID_PARAMETER;
    }
    bytesReturned = 0;
    break;
  }

  case IOCTL_GET_PID: {
    if (inputLength < sizeof(PID_PACK) || outputLength < sizeof(PID_PACK)) {
      status = STATUS_BUFFER_TOO_SMALL;
      break;
    }

    PPID_PACK pack = (PPID_PACK)inputBuffer;
    pack->name[1023] = L'\0';
    pack->pid = FindProcessIdByName(pack->name);

    if (pack->pid == 0)
      status = STATUS_NOT_FOUND;

    bytesReturned = sizeof(PID_PACK);
    break;
  }

  case IOCTL_GET_MODULE_BASE: {
    if (inputLength < sizeof(MODULE_PACK) ||
        outputLength < sizeof(MODULE_PACK)) {
      status = STATUS_BUFFER_TOO_SMALL;
      break;
    }

    PMODULE_PACK pack = (PMODULE_PACK)inputBuffer;
    pack->moduleName[1023] = L'\0';
    pack->baseAddress = FindModuleBase(pack->pid, pack->moduleName);

    if (pack->baseAddress == 0)
      status = STATUS_NOT_FOUND;

    bytesReturned = sizeof(MODULE_PACK);
    break;
  }

  default:
    status = STATUS_INVALID_DEVICE_REQUEST;
    break;
  }

  Irp->IoStatus.Status = status;
  Irp->IoStatus.Information = bytesReturned;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return status;
}

static VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
  UNICODE_STRING symLink;
  RtlInitUnicodeString(&symLink, L"\\DosDevices\\NsiCoreSys");
  IoDeleteSymbolicLink(&symLink);

  if (DriverObject->DeviceObject)
    IoDeleteDevice(DriverObject->DeviceObject);

  if (g_TargetProcess) {
    ObDereferenceObject(g_TargetProcess);
    g_TargetProcess = NULL;
  }
}

static NTSTATUS DriverInitialize(PDRIVER_OBJECT DriverObject,
                                 PUNICODE_STRING RegistryPath) {
  UNREFERENCED_PARAMETER(RegistryPath);

  UNICODE_STRING devName, symLink;
  RtlInitUnicodeString(&devName, L"\\Device\\NsiCoreSys");
  RtlInitUnicodeString(&symLink, L"\\DosDevices\\NsiCoreSys");

  // Debug: Driver initialization started
  DbgPrint("[NsiCoreSys] DriverInitialize started\n");

  // Eger onceki testten kalan bir symlink varsa once onu temizlemeyi dene
  NTSTATUS cleanupStatus = IoDeleteSymbolicLink(&symLink);
  DbgPrint("[NsiCoreSys] IoDeleteSymbolicLink status: 0x%08X\n", cleanupStatus);

  NTSTATUS status =
      IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN,
                     FILE_DEVICE_SECURE_OPEN, FALSE, &g_DeviceObject);

  DbgPrint("[NsiCoreSys] IoCreateDevice status: 0x%08X\n", status);

  if (!NT_SUCCESS(status)) {
    DbgPrint("[NsiCoreSys] Failed to create device\n");
    return status;
  }

  DbgPrint("[NsiCoreSys] Device created successfully at \\Device\\NsiCoreSys\n");

  status = IoCreateSymbolicLink(&symLink, &devName);
  DbgPrint("[NsiCoreSys] IoCreateSymbolicLink status: 0x%08X\n", status);
  if (!NT_SUCCESS(status)) {
    DbgPrint("[NsiCoreSys] Failed to create symbolic link\n");
    IoDeleteDevice(g_DeviceObject);
    return status;
  }
  DbgPrint("[NsiCoreSys] Symbolic link created: \\.\\NsiCoreSys\n");

  DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
  DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
  DriverObject->DriverUnload = DriverUnload;

  g_DeviceObject->Flags |= DO_BUFFERED_IO;
  g_DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

  return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject,
                     PUNICODE_STRING RegistryPath) {
  UNREFERENCED_PARAMETER(DriverObject);
  UNREFERENCED_PARAMETER(RegistryPath);

  UNICODE_STRING drvName;
  RtlInitUnicodeString(&drvName, L"\\Driver\\NsiCoreSys");

  return IoCreateDriver(&drvName, &DriverInitialize);
}
