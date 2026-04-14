#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <string_view>
#include <winioctl.h>

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

class Memory {
private:
  DWORD processId = 0;
  HANDLE kernelDriver = nullptr;

  typedef struct _Request {
    HANDLE process_id;
    PVOID target;
    PVOID buffer;
    SIZE_T size;
  } Request;

  typedef struct _PID_PACK {
    UINT32 pid;
    WCHAR name[1024];
  } PID_PACK;

  typedef struct _MODULE_PACK {
    UINT32 pid;
    UINT64 baseAddress;
    SIZE_T size;
    WCHAR moduleName[1024];
  } MODULE_PACK;

public:
  Memory(const std::wstring &processName) noexcept {
    std::cout
        << "[+] Attempting to connect to kernel device: \\\\.\\NsiCoreSys\n";

    kernelDriver =
        CreateFileW(L"\\\\.\\NsiCoreSys", GENERIC_READ | GENERIC_WRITE, 0,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (kernelDriver == INVALID_HANDLE_VALUE) {
      DWORD error = GetLastError();
      kernelDriver = nullptr;
      std::cout << "[!] Cannot open kernel device - Error code: " << error;
      if (error == ERROR_FILE_NOT_FOUND)
        std::cout << " (Driver not loaded)";
      else if (error == ERROR_ACCESS_DENIED)
        std::cout << " (Insufficient privileges)";
      else if (error == ERROR_INVALID_NAME)
        std::cout << " (Invalid device name)";
      else if (error == ERROR_BAD_IMPERSONATION_LEVEL)
        std::cout << " (Bad impersonation)";
      std::cout << "\n";
      return;
    }
    std::cout << "[+] Kernel device handle opened successfully\n";

    PID_PACK pidPack = {};
    wcsncpy_s(pidPack.name, processName.data(), processName.size());
    BOOL result =
        DeviceIoControl(kernelDriver, IOCTL_GET_PID, &pidPack, sizeof(pidPack),
                        &pidPack, sizeof(pidPack), nullptr, nullptr);
    if (result && pidPack.pid != 0) {
      processId = pidPack.pid;
    } else {
      std::cout << "[!] cs2.exe not found via driver\n";
      CloseHandle(kernelDriver);
      kernelDriver = nullptr;
      return;
    }

    Request attachReq = {};
    attachReq.process_id = ULongToHandle(processId);
    DeviceIoControl(kernelDriver, IOCTL_ATTACH, &attachReq, sizeof(attachReq),
                    &attachReq, sizeof(attachReq), nullptr, nullptr);

    std::cout << "[+] Kernel mode active (PID: " << processId << ")\n";
  }

  ~Memory() {
    if (kernelDriver)
      CloseHandle(kernelDriver);
  }

  bool IsConnected() const noexcept {
    return processId != 0 && kernelDriver != nullptr;
  }

  bool IsKernelMode() const noexcept { return kernelDriver != nullptr; }

  uintptr_t GetModuleBase(const std::wstring &moduleName) const {
    if (!kernelDriver)
      return 0;

    MODULE_PACK modPack = {};
    modPack.pid = processId;
    modPack.baseAddress = 0;
    wcsncpy_s(modPack.moduleName, moduleName.data(), moduleName.size());
    BOOL result = DeviceIoControl(kernelDriver, IOCTL_GET_MODULE_BASE, &modPack,
                                  sizeof(modPack), &modPack, sizeof(modPack),
                                  nullptr, nullptr);
    if (result)
      return static_cast<std::uintptr_t>(modPack.baseAddress);
    return 0;
  }

  template <typename T>
  const T read(const std::uintptr_t address) const noexcept {
    T value = {};
    if (!kernelDriver || processId == 0)
      return value;
    if (address == 0 || address >= 0x7FFFFFFFFFFF)
      return value;

    Request readReq = {};
    readReq.process_id = ULongToHandle(processId);
    readReq.target = reinterpret_cast<PVOID>(address);
    readReq.buffer = &value;
    readReq.size = sizeof(T);
    DeviceIoControl(kernelDriver, IOCTL_READ, &readReq, sizeof(readReq),
                    &readReq, sizeof(readReq), nullptr, nullptr);
    return value;
  }

  struct WriteRequest {
    PVOID target;
    SIZE_T size;
    UCHAR data[256];
  };

  template <typename T>
  void write(const std::uintptr_t address, const T &value) const noexcept {
    static_assert(sizeof(T) <= 256, "write: type too large");
    if (!kernelDriver || processId == 0)
      return;
    if (address == 0 || address >= 0x7FFFFFFFFFFF)
      return;

    WriteRequest req = {};
    req.target = reinterpret_cast<PVOID>(address);
    req.size = sizeof(T);
    memcpy(req.data, &value, sizeof(T));
    DeviceIoControl(kernelDriver, IOCTL_WRITE, &req, sizeof(req), nullptr, 0,
                    nullptr, nullptr);
  }
};
