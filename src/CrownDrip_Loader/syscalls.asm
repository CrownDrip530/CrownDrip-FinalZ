.code
; 繞過 ntdll.dll，直接向 Windows 內核申請 PAGE_EXECUTE_READWRITE 權限的匿名記憶體頁
NtAllocateVirtualMemoryProc PROC
    mov r10, rcx
    mov eax, 18h            ; Windows 10/11 的動態分配記憶體系統呼叫號 (Syscall Number)
    syscall                 ; 直接進入 Ring 0 內核層執行
    ret
NtAllocateVirtualMemoryProc ENDP
END
