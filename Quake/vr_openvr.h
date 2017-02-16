#ifndef __VR_OPENVR_H
#define __VR_OPENVR_H

#include "quakedef.h"

#ifdef EXTERN_C
// HACK: windows.h apparently defines this, so undef it so it doesn't complain
#undef EXTERN_C
#endif

#include <openvr_capi.h>

// openvr_capi.h doesn't export these for some reason
// so we gotta do it ourselves

typedef struct VR_IVRApplications_FnTable VR_IVRApplications_FnTable_t;
typedef struct VR_IVRChaperoneSetup_FnTable VR_IVRChaperoneSetup_FnTable_t;
typedef struct VR_IVRChaperone_FnTable VR_IVRChaperone_FnTable_t;
typedef struct VR_IVRCompositor_FnTable VR_IVRCompositor_FnTable_t;
typedef struct VR_IVRExtendedDisplay_FnTable VR_IVRExtendedDisplay_FnTable_t;
typedef struct VR_IVRNotifications_FnTable VR_IVRNotifications_FnTable_t;
typedef struct VR_IVROverlay_FnTable VR_IVROverlay_FnTable_t;
typedef struct VR_IVRRenderModels_FnTable VR_IVRRenderModels_FnTable_t;
typedef struct VR_IVRResources_FnTable VR_IVRResources_FnTable_t;
typedef struct VR_IVRScreenshots_FnTable VR_IVRScreenshots_FnTable_t;
typedef struct VR_IVRSettings_FnTable VR_IVRSettings_FnTable_t;
typedef struct VR_IVRSystem_FnTable VR_IVRSystem_FnTable_t;
typedef struct VR_IVRTrackedCamera_FnTable VR_IVRTrackedCamera_FnTable_t;

#if defined( _WIN32 ) && !defined( _X360 )
#if defined( OPENVR_API_EXPORTS )
#define Q_API __declspec( dllexport )
#elif defined( OPENVR_API_NODLL )
#define Q_API
#else
#define Q_API __declspec( dllimport ) 
#endif // OPENVR_API_EXPORTS
#elif defined( __GNUC__ )
#if defined( OPENVR_API_EXPORTS )
#define Q_API __attribute__ ((visibility("default")))
#else
#define Q_API
#endif // OPENVR_API_EXPORTS
#else // !WIN32
#define Q_API
#endif

Q_API intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
Q_API extern void VR_ShutdownInternal();
Q_API bool VR_IsHmdPresent();
Q_API bool VR_IsRuntimeInstalled();
Q_API const char* VR_GetStringForHmdError(EVRInitError error);
Q_API intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
Q_API bool VR_IsInterfaceVersionValid(const char *pchInterfaceVersion);
Q_API uint32_t VR_GetInitToken();
Q_API char* VR_RuntimePath();
Q_API const char * VR_GetVRInitErrorAsSymbol(EVRInitError error);

VR_IVRSystem_FnTable_t* OpenVR_Init(EVRInitError *peError, EVRApplicationType eType);
void OpenVR_Shutdown(void);

VR_IVRSystem_FnTable_t* VRSystem(void);
VR_IVRCompositor_FnTable_t* VRCompositor(void);

#endif
