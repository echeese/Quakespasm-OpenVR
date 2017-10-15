#include "vr_openvr.h"

VR_IVRApplications_FnTable_t* pVRApplications;
VR_IVRChaperoneSetup_FnTable_t* pVRChaperoneSetup;
VR_IVRChaperone_FnTable_t* pVRChaperone;
VR_IVRCompositor_FnTable_t* pVRCompositor;
VR_IVRExtendedDisplay_FnTable_t* pVRExtendedDisplay;
VR_IVRNotifications_FnTable_t* pVRNotifications;
VR_IVROverlay_FnTable_t* pVROverlay;
VR_IVRRenderModels_FnTable_t* pVRRenderModels;
VR_IVRResources_FnTable_t* pVRResources;
VR_IVRScreenshots_FnTable_t* pVRScreenshots;
VR_IVRSettings_FnTable_t* pVRSettings;
VR_IVRSystem_FnTable_t* pVRSystem;
VR_IVRTrackedCamera_FnTable_t* pVRTrackedCamera;

static intptr_t VRToken;

void Clear(void)
{
	Con_DPrintf("Clearing VR interfaces");
	pVRSystem = NULL;
	pVRCompositor = NULL;
}

void CheckClear(void)
{
	if (VRToken != VR_GetInitToken())
	{
		Clear();
		VRToken = VR_GetInitToken();
	}
}

VR_IVRSystem_FnTable_t* OpenVR_Init(EVRInitError *peError, EVRApplicationType eType)
{
	EVRInitError eError;
	VRToken = VR_InitInternal(&eError, eType);
	Clear();
	if (eError == EVRInitError_VRInitError_None)
	{
		if (VR_IsInterfaceVersionValid(IVRSystem_Version))
		{
			pVRSystem = VRSystem();
		}
		else
		{
			VR_ShutdownInternal();
			eError = EVRInitError_VRInitError_Init_InterfaceNotFound;
		}
	}

	if (peError)
	{
		*peError = eError;
	}
	return pVRSystem;
}

void OpenVR_Shutdown(void)
{
	VR_ShutdownInternal();
}

VR_IVRSystem_FnTable_t* VRSystem(void)
{
	CheckClear();
	if (!pVRSystem)
	{
		EVRInitError eError = EVRInitError_VRInitError_None;
		char fnTableName[128];
		sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
		VR_IVRSystem_FnTable_t* pInterface = (VR_IVRSystem_FnTable_t*) VR_GetGenericInterface(fnTableName, &eError);
		if (pInterface && eError == EVRInitError_VRInitError_None)
		{
			pVRSystem = pInterface;
		}
		else
		{
			Sys_Error("[OpenVR] Unable to get system interface: %s\n", VR_GetVRInitErrorAsEnglishDescription(eError));
		}
	}
	return pVRSystem;
}

VR_IVRCompositor_FnTable_t* VRCompositor(void)
{
	CheckClear();
	if (!pVRCompositor)
	{
		EVRInitError eError = EVRInitError_VRInitError_None;
		char fnTableName[128];
		sprintf(fnTableName, "FnTable:%s", IVRCompositor_Version);
		VR_IVRCompositor_FnTable_t* pInterface = (VR_IVRCompositor_FnTable_t*) VR_GetGenericInterface(fnTableName, &eError);
		if (pInterface && eError == EVRInitError_VRInitError_None)
		{
			pVRCompositor = pInterface;
		}
		else
		{
			Sys_Error("[OpenVR] Unable to get compositor interface: %s\n", VR_GetVRInitErrorAsEnglishDescription(eError));
		}
	}
	return pVRCompositor;
}