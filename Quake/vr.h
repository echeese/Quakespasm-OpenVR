#ifndef __VR_H
#define __VR_H

#include <vr_openvr.h>

// k_unMaxTrackedDeviceCount
#define MAX_TRACKED_DEVICE_COUNT 16

extern cvar_t vr_enable;

typedef struct FramebufferDesc
{
	GLuint m_nDepthBufferId;
	GLuint m_nRenderTextureId;
	GLuint m_nRenderFramebufferId;
	GLuint m_nResolveTextureId;
	GLuint m_nResolveFramebufferId;
} FramebufferDesc_t;

int vr_width, vr_height;

void VR_Init(void);
void VR_Shutdown(void);
qboolean VR_Enable(void);
void VR_Disable(void);
void VR_Submit(void);
void VR_UpdatePoses(void);
void VR_RenderScene(void);
void VR_SetupGL(void);

#endif