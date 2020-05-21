#include "pch.h"
#include "tdriver.h"



BOOLEAN TdProcessNotifyRoutineSet2 = FALSE;
DRIVER_INITIALIZE  DriverEntry;

_Dispatch_type_(IRP_MJ_CREATE) DRIVER_DISPATCH TdDeviceCreate;

DRIVER_UNLOAD   TdDeviceUnload;

typedef struct _OSR_WORK_ITEM 
{
    WORK_QUEUE_ITEM    WorkItem;
    PVOID              Process;

} OSR_WORK_ITEM, * POSR_WORK_ITEM;


int MitigationFlags = 0;
char SignatureLevel = 0;
char SectionSignatureLevel = 0;
PVOID vmwpProcess;
VOID WorkRoutine(PVOID Parameter)
{

    ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
    POSR_WORK_ITEM OsrWorkItem = (POSR_WORK_ITEM)Parameter;
    LARGE_INTEGER delay;

    delay.QuadPart = -10000 * 2000;
//    KeDelayExecutionThread(KernelMode, FALSE, &delay);


    if (STATUS_SUCCESS == KeDelayExecutionThread(KernelMode, FALSE, &delay))
    {
        vmwpProcess = OsrWorkItem->Process;
		
        memset(((char*)OsrWorkItem->Process) + 0x850, 0, sizeof(int));  // +0x850 MitigationFlags  : Uint4B
        memset(((char*)OsrWorkItem->Process) + 0x6f8, 0, sizeof(char));  // +0x6f8 SignatureLevel   : UChar
        memset(((char*)OsrWorkItem->Process) + 0x6f9, 0, sizeof(char));  //  + 0x6f9 SectionSignatureLevel : UChar
    }

    ExFreePool(OsrWorkItem);
}



VOID
TdCreateProcessNotifyRoutine2 (
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _In_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
    )
{
    UNREFERENCED_PARAMETER(ProcessId);

   
    if (CreateInfo != NULL)
    {
        UNICODE_STRING vmwp = RTL_CONSTANT_STRING(L"\\??\\C:\\Windows\\System32\\vmwp.exe");
        if (RtlCompareUnicodeString(&vmwp, CreateInfo->ImageFileName, TRUE) == 0)
        {
            POSR_WORK_ITEM OsrWorkItem;
            OsrWorkItem = ExAllocatePool(NonPagedPool, sizeof(OSR_WORK_ITEM));
            OsrWorkItem->Process = (PVOID)Process;
            ExInitializeWorkItem(&OsrWorkItem->WorkItem, WorkRoutine, OsrWorkItem);
            ExQueueWorkItem(&OsrWorkItem->WorkItem, DelayedWorkQueue);

        }
    }

}


NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS Status;
    UNREFERENCED_PARAMETER (RegistryPath);

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = TdDeviceCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = TdDeviceCreate;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]        = TdDeviceCreate;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = TdDeviceCreate;
    DriverObject->DriverUnload                         = TdDeviceUnload;
    
    Status = PsSetCreateProcessNotifyRoutineEx (TdCreateProcessNotifyRoutine2,FALSE );

    if (!NT_SUCCESS(Status))
    {
        ///__debugbreak();
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "ObCallbackTest: DriverEntry: PsSetCreateProcessNotifyRoutineEx(2) returned 0x%x\n", Status);
        return Status;
    }

    TdProcessNotifyRoutineSet2 = TRUE;
    return Status;
}

VOID
TdDeviceUnload (
    _In_ PDRIVER_OBJECT DriverObject
)
{
    NTSTATUS Status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(DriverObject);

    if (TdProcessNotifyRoutineSet2 == TRUE)
    {
        Status = PsSetCreateProcessNotifyRoutineEx (TdCreateProcessNotifyRoutine2, TRUE );
    }

}


NTSTATUS
TdDeviceCreate (
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp
)
{
    UNREFERENCED_PARAMETER (DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

