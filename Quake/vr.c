#include "vr.h"
#include "vr_openvr.h"

cvar_t vr_enable = { "vr_enable", "1", CVAR_NONE };

FramebufferDesc_t VR_framebuffer;

qboolean initialized = false;

TrackedDevicePose_t trackedDevicePose[MAX_TRACKED_DEVICE_COUNT];

qboolean CreateFrameBuffer(int nWidth, int nHeight, FramebufferDesc_t* framebufferDesc)
{
	if (!gl_renderbuffers_able)
		return false;

	GL_GenFramebuffersFunc(1, &framebufferDesc->m_nRenderFramebufferId);
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebufferDesc->m_nRenderFramebufferId);

	GL_GenRenderbuffersFunc(1, &framebufferDesc->m_nDepthBufferId);
	GL_BindRenderbufferFunc(GL_RENDERBUFFER, framebufferDesc->m_nDepthBufferId);
	GL_RenderbufferStorageMultisampleFunc(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, nWidth, nHeight);
	GL_FramebufferRenderbufferFunc(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebufferDesc->m_nDepthBufferId);

	glGenTextures(1, &framebufferDesc->m_nRenderTextureId);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc->m_nRenderTextureId);
	GL_TexImage2DMultisampleFunc(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, nWidth, nHeight, true);
	GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc->m_nRenderTextureId, 0);

	GL_GenFramebuffersFunc(1, &framebufferDesc->m_nResolveFramebufferId);
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebufferDesc->m_nResolveFramebufferId);

	glGenTextures(1, &framebufferDesc->m_nResolveTextureId);
	glBindTexture(GL_TEXTURE_2D, framebufferDesc->m_nResolveTextureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc->m_nResolveTextureId, 0);

	// check FBO status
	GLenum status = GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
		return false;

	GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);

	return true;
}

static void VR_Enabled_f(cvar_t *var)
{
	if (!vr_enable.value)
	{
		VR_Disable();
		return;
	}

	if (!VR_Enable())
		Cvar_SetValueQuick(&vr_enable, 0);
}

void VR_Init (void)
{
	Cvar_RegisterVariable(&vr_enable);
	Cvar_SetCallback(&vr_enable, VR_Enabled_f);

	if (vr_enable.value)
		VR_Enable();
}

void VR_Shutdown(void)
{
	VR_Disable();
}

qboolean VR_Enable(void)
{
	if (initialized)
		return true;

	EVRInitError err;
	OpenVR_Init(&err, EVRApplicationType_VRApplication_Scene);

	if (err != EVRInitError_VRInitError_None)
	{
		Con_Warning("[VR] Unable to init VR runtime: %s\n", VR_GetStringForHmdError(err));
		return false;
	}

	VRSystem()->GetRecommendedRenderTargetSize(&vr_width, &vr_height);
	if (!CreateFrameBuffer(vr_width * 2, vr_height, &VR_framebuffer))
	{
		Con_Warning("[VR] Unable to create framebuffer\n");
		return false;
	}

	Con_Printf("OpenVR Initialized\n");

	initialized = true;
	return true;
}

void VR_Disable(void)
{
	if (!initialized)
		return;

	GL_DeleteRenderbuffersFunc(1, &VR_framebuffer.m_nDepthBufferId);
	glDeleteTextures(1, &VR_framebuffer.m_nRenderTextureId);
	GL_DeleteFramebuffersFunc(1, &VR_framebuffer.m_nRenderFramebufferId);
	glDeleteTextures(1, &VR_framebuffer.m_nResolveTextureId);
	GL_DeleteFramebuffersFunc(1, &VR_framebuffer.m_nResolveFramebufferId);

	OpenVR_Shutdown();
	initialized = false;
}

void VR_Submit (void)
{
	Texture_t tex =
	{
		.handle = (void*)(uintptr_t)VR_framebuffer.m_nResolveTextureId,
		.eType = ETextureType_TextureType_OpenGL,
		.eColorSpace = EColorSpace_ColorSpace_Auto
	};

	VRTextureBounds_t left_bounds = { 0.0f, 0.0f, 0.5f, 1.0f };
	VRTextureBounds_t right_bounds = { 0.5f, 0.0f, 1.0f, 1.0f };
	VRCompositor()->Submit(EVREye_Eye_Left, &tex, &left_bounds, EVRSubmitFlags_Submit_Default);
	VRCompositor()->Submit(EVREye_Eye_Right, &tex, &right_bounds, EVRSubmitFlags_Submit_Default);
}

void VR_UpdatePoses(void)
{
	if (!initialized)
		return;
	VRCompositor()->WaitGetPoses(trackedDevicePose, MAX_TRACKED_DEVICE_COUNT, NULL, 0);
}

void VR_RenderView(void)
{
	int oldwidth = glwidth;
	int oldheight = glheight;

	glwidth = vr_width;
	glheight = vr_height;

	r_refdef.fov_x = 110;
	r_refdef.fov_y = 100;

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_MULTISAMPLE);

	// Left Eye
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, VR_framebuffer.m_nRenderFramebufferId);
	glViewport(0, 0, vr_width, vr_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	V_RenderView();
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
	glDisable(GL_MULTISAMPLE);

	GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, VR_framebuffer.m_nRenderFramebufferId);
	GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, VR_framebuffer.m_nResolveFramebufferId);

	GL_BlitFramebufferFunc(0, 0, vr_width, vr_height, 0, 0, vr_width, vr_height,
	GL_COLOR_BUFFER_BIT,
	GL_LINEAR);

	GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
	GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, 0);

	glEnable(GL_MULTISAMPLE);

	srand((int)(cl.time * 1000));
	// Right Eye
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, VR_framebuffer.m_nRenderFramebufferId);
	glViewport(vr_width, 0, vr_width, vr_height);
	V_RenderView();
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);

	glDisable(GL_MULTISAMPLE);

	GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, VR_framebuffer.m_nRenderFramebufferId);
	GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, VR_framebuffer.m_nResolveFramebufferId);

	GL_BlitFramebufferFunc(vr_width, 0, vr_width*2, vr_height, vr_width, 0, vr_width*2, vr_height,
	GL_COLOR_BUFFER_BIT,
	GL_LINEAR);

	GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
	GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, 0);

	glwidth = oldwidth;
	glheight = oldheight;
}
