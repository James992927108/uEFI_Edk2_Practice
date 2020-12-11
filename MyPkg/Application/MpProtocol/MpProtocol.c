#include "MpProtocol.h"

EFI_MP_SERVICES_PROTOCOL *mCpuFeaturesMpServices = NULL;

/**
  Worker function to get EFI_MP_SERVICES_PROTOCOL pointer.

  @return Pointer to EFI_MP_SERVICES_PROTOCOL.
**/
EFI_MP_SERVICES_PROTOCOL *GetMpProtocol(VOID)
{
	EFI_STATUS Status;
	if (mCpuFeaturesMpServices == NULL)
	{
		Status = gBS->LocateProtocol(
			&gEfiMpServiceProtocolGuid,
			NULL,
			(VOID **)&mCpuFeaturesMpServices);
		ASSERT_EFI_ERROR(Status);
	}

	ASSERT(mCpuFeaturesMpServices != NULL);
	return mCpuFeaturesMpServices;
}

VOID PrintMicroCodeVersion(IN OUT VOID *Buffer)
{
	MSR_IA32_BIOS_SIGN_ID_REGISTER BiosSignIdMsr;
	AsmWriteMsr64(MSR_IA32_BIOS_SIGN_ID, 0);
	AsmCpuid(CPUID_VERSION_INFO, NULL, NULL, NULL, NULL);
	BiosSignIdMsr.Uint64 = AsmReadMsr64(MSR_IA32_BIOS_SIGN_ID);
	Print(L"=> MicroCodeVersion 0x%08x\n", BiosSignIdMsr.Bits.MicrocodeUpdateSignature);
}

VOID GetWhoAmI(IN OUT VOID *Buffer)
{
	EFI_STATUS Status;
	UINTN ProcessorIndex;
	EFI_MP_SERVICES_PROTOCOL *MpServices;
	MpServices = GetMpProtocol();
	Status = MpServices->WhoAmI(MpServices, &ProcessorIndex);
	ASSERT_EFI_ERROR(Status);
	Print(L"Current Process Index is %d\n", ProcessorIndex);
}

EFI_STATUS WakeupAllAps()
{
	EFI_STATUS Status;
	EFI_MP_SERVICES_PROTOCOL *MpServices;

	UINTN *FailedCpuList = NULL;
	// Wakeup all APs
	Print(L"Start WakeupAllAps()\n");
	MpServices = GetMpProtocol();
	Status = MpServices->StartupAllAPs(
		MpServices,
		PrintMicroCodeVersion,
		TRUE,
		NULL,
		0,
		NULL,
		&FailedCpuList);

	// ASSERT_EFI_ERROR(Status);
	if (EFI_ERROR(Status))
	{
		Print(L"MpServices->StartupAllAPs Error\n");
		return EFI_UNSUPPORTED;
	}

	if (FailedCpuList == 0)
	{
		Print(L"All APs finish successfully\n");
	}
	else{
		Print(L"All APs fail, FailedCpuList %d\n", FailedCpuList);
	}

	Print(L"Finish WakeupAllAps()\n");
	return EFI_SUCCESS;
}

UINTN PrintNumberOfProcessors(VOID)
{
	EFI_STATUS Status;
	EFI_MP_SERVICES_PROTOCOL *MpServices;
	MpServices = GetMpProtocol();
	UINTN NumOfProcessors;
	UINTN NumberOfEnabledProcessors;
	// Determine number of processors
	Status = MpServices->GetNumberOfProcessors(MpServices, &NumOfProcessors, &NumberOfEnabledProcessors);

	if (EFI_ERROR(Status))
	{
		Print(L"MpServices->GetNumEnabledProcessors:Unable to determine number of processors\n");
		return EFI_UNSUPPORTED;
	}
	Print(L"Number of Processors %d\n", NumOfProcessors);
	Print(L"Number of Enabled Processors %d\n", NumberOfEnabledProcessors);
	return NumOfProcessors;
}

VOID PrintProcessorInfo(IN UINTN NumOfProcessors)
{
	EFI_STATUS Status;
	EFI_MP_SERVICES_PROTOCOL *MpServices;
	MpServices = GetMpProtocol();

	EFI_PROCESSOR_INFORMATION ProcessorInfo;
	//Get more information by GetProcessorInfo
	for (UINTN i = 0; i < NumOfProcessors; i++)
	{
		Status = MpServices->GetProcessorInfo(MpServices, i, &ProcessorInfo);
		Print(L"Prcoessor #%d Local APIC ID = %lX, Flags = %x, Package = %x, Core = %x, Thread = %x \n",
			  i,
			  ProcessorInfo.ProcessorId,
			  ProcessorInfo.StatusFlag,
			  ProcessorInfo.Location.Package,
			  ProcessorInfo.Location.Core,
			  ProcessorInfo.Location.Thread);
	}
}

UINT32 GetLargestStanardFunction()
{
	UINT32 Eax;
	AsmCpuid(CPUID_STANDARD_FUNCTION_ADDR, &Eax, 0, 0, 0);
	// Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax, 0, 0, 0);
	// PRINT_VALUE(Eax, MaximumBasicFunction);
	return Eax;
}

VOID PrintCpuFeatureInfo()
{
	// Processor Signature
	CPUID_VERSION_INFO_EAX Eax;
	// Misc info
	CPUID_VERSION_INFO_EBX Ebx;
	// Feature Flags
	CPUID_VERSION_INFO_ECX Ecx;
	// Feature Flags
	CPUID_VERSION_INFO_EDX Edx;

	UINT32 DisplayFamily;
	UINT32 DisplayModel;

	Print(L"FUNCTION (Leaf %08x)\n", CPUID_VERSION_INFO);
	if (CPUID_VERSION_INFO > GetLargestStanardFunction())
		return;

	AsmCpuid(CPUID_VERSION_INFO, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);

	Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);

	//CPUID201205-5.1.2.2
	DisplayFamily = Eax.Bits.FamilyId;
	DisplayFamily |= (Eax.Bits.ExtendedFamilyId << 4);

	DisplayModel = Eax.Bits.Model;
	DisplayModel |= (Eax.Bits.ExtendedModelId << 4);

	Print(L"EAX:  Type = 0x%x  Family = 0x%x  Model = 0x%x  Stepping = 0x%x\n", Eax.Bits.ProcessorType, DisplayFamily, DisplayModel, Eax.Bits.SteppingId);
}

EFI_STATUS EFIAPI main(IN int Argc, IN CHAR16 **Argv)
{
	EFI_STATUS Status;
	Status = WakeupAllAps();
	if (!EFI_ERROR(Status))
	{
		UINTN NumOfProcessors;
		NumOfProcessors = PrintNumberOfProcessors();
		PrintProcessorInfo(NumOfProcessors);
		PrintCpuFeatureInfo();
	}
	return EFI_SUCCESS;
}
