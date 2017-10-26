#include "vr.h"
#include "vr_openvr.h"
#include "quakedef.h"

// number of Quake units per meter
// 1 unit = 1.5 inches = 0.0381 meters
// 1 / 0.0381 = 26.2467
cvar_t vr_scale = { "vr_scale", "26.2467", CVAR_NONE };

cvar_t vr_enable = { "vr_enable", "1", CVAR_NONE };
float vr_yaw;

FramebufferDesc_t VR_framebuffers[2];
qboolean vr_initialized = false;
TrackedDevicePose_t trackedDevicePose[MAX_TRACKED_DEVICE_COUNT];
EVREye current_eye;

// Forward declarations
qboolean gluInvertMatrix(const float m[16], float invOut[16]);
void transpose44(float matrix[4][4], float matrix2[4][4]);
void transpose34to44(float mat34[3][4], float mat44[4][4]);
void GL_SetFrustum(float fovx, float fovy);
cvar_t gl_farclip;

// from gl_rmain.c
#define NEARCLIP 4

const int msaa_samples = 8;
static qboolean CreateFrameBuffer(int nWidth, int nHeight, FramebufferDesc_t* framebufferDesc)
{
	if (!gl_renderbuffers_able)
		return false;

	GL_GenFramebuffersFunc(1, &framebufferDesc->m_nRenderFramebufferId);
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebufferDesc->m_nRenderFramebufferId);

	GL_GenRenderbuffersFunc(1, &framebufferDesc->m_nDepthBufferId);
	GL_BindRenderbufferFunc(GL_RENDERBUFFER, framebufferDesc->m_nDepthBufferId);
	GL_RenderbufferStorageMultisampleFunc(GL_RENDERBUFFER, msaa_samples, GL_DEPTH_COMPONENT, nWidth, nHeight);
	GL_FramebufferRenderbufferFunc(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebufferDesc->m_nDepthBufferId);

	glGenTextures(1, &framebufferDesc->m_nRenderTextureId);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc->m_nRenderTextureId);
	GL_TexImage2DMultisampleFunc(GL_TEXTURE_2D_MULTISAMPLE, msaa_samples, GL_RGBA8, nWidth, nHeight, true);
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

static qboolean CreateFrameBuffers(int nWidth, int nHeight)
{
	return CreateFrameBuffer(nWidth, nHeight, &VR_framebuffers[0]) && CreateFrameBuffer(nWidth, nHeight, &VR_framebuffers[1]);
}

static qboolean DestroyFrameBuffer(FramebufferDesc_t* framebufferDesc)
{
	GL_DeleteRenderbuffersFunc(1, &framebufferDesc->m_nDepthBufferId);
	glDeleteTextures(1, &framebufferDesc->m_nRenderTextureId);
	GL_DeleteFramebuffersFunc(1, &framebufferDesc->m_nRenderFramebufferId);
	glDeleteTextures(1, &framebufferDesc->m_nResolveTextureId);
	GL_DeleteFramebuffersFunc(1, &framebufferDesc->m_nResolveFramebufferId);
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
	Cvar_RegisterVariable(&vr_scale);
	Cvar_RegisterVariable(&vr_enable);
	Cvar_SetCallback(&vr_enable, VR_Enabled_f);

	if (vr_enable.value && !VR_Enable())
		Cvar_SetValueQuick(&vr_enable, 0);
}

void VR_Shutdown(void)
{
	VR_Disable();
}

qboolean VR_Enable(void)
{
	if (vr_initialized)
		return true;

	EVRInitError err;
	OpenVR_Init(&err, EVRApplicationType_VRApplication_Scene);

	if (err != EVRInitError_VRInitError_None)
	{
		Con_Warning("[VR] Unable to init VR runtime: %s\n", VR_GetVRInitErrorAsEnglishDescription(err));
		return false;
	}

	VRSystem()->GetRecommendedRenderTargetSize(&vr_width, &vr_height);
	if (!CreateFrameBuffers(vr_width, vr_height))
	{
		Con_Warning("[VR] Unable to create framebuffer\n");
		return false;
	}

	Con_Printf("OpenVR Initialized\n");

	vr_initialized = true;
	return true;
}

void VR_Disable(void)
{
	if (!vr_initialized)
		return;

	DestroyFrameBuffer(&VR_framebuffers[0]);
	DestroyFrameBuffer(&VR_framebuffers[1]);

	OpenVR_Shutdown();
	vr_initialized = false;
}

void VR_Submit (void)
{
	if (!vr_initialized)
		return;

	Texture_t tex =
	{
		.handle = (void*)(uintptr_t)VR_framebuffers[0].m_nResolveTextureId,
		.eType = ETextureType_TextureType_OpenGL,
		.eColorSpace = EColorSpace_ColorSpace_Auto
	};

	VRCompositor()->Submit(EVREye_Eye_Left, &tex, NULL, EVRSubmitFlags_Submit_Default);
	tex.handle = (void*)(uintptr_t)VR_framebuffers[1].m_nResolveTextureId;
	VRCompositor()->Submit(EVREye_Eye_Right, &tex, NULL, EVRSubmitFlags_Submit_Default);
}

void VR_PollEvents(void)
{
	struct VREvent_t vrevent;
	while (VRSystem()->PollNextEvent(&vrevent, sizeof vrevent))
	{
		switch (vrevent.eventType)
		{

		}
	}
}

void VR_UpdatePoses(void)
{
	if (!vr_initialized)
		return;
	VRCompositor()->WaitGetPoses(trackedDevicePose, MAX_TRACKED_DEVICE_COUNT, NULL, 0);
	VR_PollEvents();
}

void R_RenderScene(void);

void VR_RenderScene(void)
{
	int oldwidth = glwidth;
	int oldheight = glheight;

	glwidth = vr_width;
	glheight = vr_height;

	r_refdef.fov_x = 150;
	r_refdef.fov_y = 150;

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	glEnable(GL_MULTISAMPLE);

	// Left Eye
	current_eye = EVREye_Eye_Left;
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, VR_framebuffers[0].m_nRenderFramebufferId);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	R_RenderScene();
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
	glDisable(GL_MULTISAMPLE);

	GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, VR_framebuffers[0].m_nRenderFramebufferId);
	GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, VR_framebuffers[0].m_nResolveFramebufferId);

	GL_BlitFramebufferFunc(0, 0, vr_width, vr_height, 0, 0, vr_width, vr_height,
		GL_COLOR_BUFFER_BIT,
		GL_LINEAR);

	GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
	GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, 0);

	glEnable(GL_MULTISAMPLE);

	srand((int)(cl.time * 1000));
	// Right Eye
	current_eye = EVREye_Eye_Right;
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, VR_framebuffers[1].m_nRenderFramebufferId);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	R_RenderScene();
	GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);

	glDisable(GL_MULTISAMPLE);

	GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, VR_framebuffers[1].m_nRenderFramebufferId);
	GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, VR_framebuffers[1].m_nResolveFramebufferId);

	GL_BlitFramebufferFunc(0, 0, vr_width, vr_height, 0, 0, vr_width, vr_height,
		GL_COLOR_BUFFER_BIT,
		GL_LINEAR);

	GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
	GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, 0);

	glwidth = oldwidth;
	glheight = oldheight;
}

void VR_SetupGL(void)
{
	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, vr_width, vr_height);

	struct HmdMatrix44_t mat = VRSystem()->GetProjectionMatrix(current_eye, NEARCLIP, gl_farclip.value);
	float mat2[4][4];
	// OpenVR and OpenGL have different matrix column ordering so we must transpose
	transpose44(mat.m, mat2);
	glLoadMatrixf(&mat2[0][0]);

	glMatrixMode(GL_MODELVIEW);
	HmdMatrix34_t eyeMatrix = VRSystem()->GetEyeToHeadTransform(current_eye);
	float eyeMatrix2[4][4] = { 0 };
	// convert to 4x4 matrix, and transpose for same reason as above
	transpose34to44(eyeMatrix.m, eyeMatrix2);

	// Scale position to quake units
	eyeMatrix2[3][0] *= vr_scale.value;
	eyeMatrix2[3][1] *= vr_scale.value;
	eyeMatrix2[3][2] *= vr_scale.value;
	gluInvertMatrix(&eyeMatrix2[0][0], &eyeMatrix2[0][0]);
	glLoadMatrixf(&eyeMatrix2[0][0]);

	float headMatrix[4][4];
	transpose34to44(trackedDevicePose[0].mDeviceToAbsoluteTracking.m, headMatrix);
	headMatrix[3][0] *= vr_scale.value;
	headMatrix[3][1] *= vr_scale.value;
	headMatrix[3][2] *= vr_scale.value;
	gluInvertMatrix(&headMatrix[0][0], &headMatrix[0][0]);
	glMultMatrixf(&headMatrix[0][0]);

	glRotatef(-90.0f - vr_yaw, 0.0f, 1.0f, 0.0f);

	glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
	glTranslatef(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2] + cl.viewheight + 24.0f);
	// FIXME: I don't like hardcoding the distance to the bottom of the player's hull (24.0f) here,
	// but it seems impossible to get the information about this solely from the client, as NetQuake
	// only seems to send the origin information, and nothing about the bounding box. Oh well.


	//
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
}

// Spawning and teleporting are a few things that change the player's angles
// This is so whenever you spawn, you always face the intended direction.
void VR_SetYaw(float yaw)
{
	vr_yaw = yaw;
}

// BORING MATH STUFF

// Lifted and adapted from https://stackoverflow.com/a/1148405/401203
qboolean gluInvertMatrix(const float m[16], float invOut[16])
{
	float inv[16], det;
	int i;

	inv[0] = m[5] * m[10] * m[15] -
		m[5] * m[11] * m[14] -
		m[9] * m[6] * m[15] +
		m[9] * m[7] * m[14] +
		m[13] * m[6] * m[11] -
		m[13] * m[7] * m[10];

	inv[4] = -m[4] * m[10] * m[15] +
		m[4] * m[11] * m[14] +
		m[8] * m[6] * m[15] -
		m[8] * m[7] * m[14] -
		m[12] * m[6] * m[11] +
		m[12] * m[7] * m[10];

	inv[8] = m[4] * m[9] * m[15] -
		m[4] * m[11] * m[13] -
		m[8] * m[5] * m[15] +
		m[8] * m[7] * m[13] +
		m[12] * m[5] * m[11] -
		m[12] * m[7] * m[9];

	inv[12] = -m[4] * m[9] * m[14] +
		m[4] * m[10] * m[13] +
		m[8] * m[5] * m[14] -
		m[8] * m[6] * m[13] -
		m[12] * m[5] * m[10] +
		m[12] * m[6] * m[9];

	inv[1] = -m[1] * m[10] * m[15] +
		m[1] * m[11] * m[14] +
		m[9] * m[2] * m[15] -
		m[9] * m[3] * m[14] -
		m[13] * m[2] * m[11] +
		m[13] * m[3] * m[10];

	inv[5] = m[0] * m[10] * m[15] -
		m[0] * m[11] * m[14] -
		m[8] * m[2] * m[15] +
		m[8] * m[3] * m[14] +
		m[12] * m[2] * m[11] -
		m[12] * m[3] * m[10];

	inv[9] = -m[0] * m[9] * m[15] +
		m[0] * m[11] * m[13] +
		m[8] * m[1] * m[15] -
		m[8] * m[3] * m[13] -
		m[12] * m[1] * m[11] +
		m[12] * m[3] * m[9];

	inv[13] = m[0] * m[9] * m[14] -
		m[0] * m[10] * m[13] -
		m[8] * m[1] * m[14] +
		m[8] * m[2] * m[13] +
		m[12] * m[1] * m[10] -
		m[12] * m[2] * m[9];

	inv[2] = m[1] * m[6] * m[15] -
		m[1] * m[7] * m[14] -
		m[5] * m[2] * m[15] +
		m[5] * m[3] * m[14] +
		m[13] * m[2] * m[7] -
		m[13] * m[3] * m[6];

	inv[6] = -m[0] * m[6] * m[15] +
		m[0] * m[7] * m[14] +
		m[4] * m[2] * m[15] -
		m[4] * m[3] * m[14] -
		m[12] * m[2] * m[7] +
		m[12] * m[3] * m[6];

	inv[10] = m[0] * m[5] * m[15] -
		m[0] * m[7] * m[13] -
		m[4] * m[1] * m[15] +
		m[4] * m[3] * m[13] +
		m[12] * m[1] * m[7] -
		m[12] * m[3] * m[5];

	inv[14] = -m[0] * m[5] * m[14] +
		m[0] * m[6] * m[13] +
		m[4] * m[1] * m[14] -
		m[4] * m[2] * m[13] -
		m[12] * m[1] * m[6] +
		m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] +
		m[1] * m[7] * m[10] +
		m[5] * m[2] * m[11] -
		m[5] * m[3] * m[10] -
		m[9] * m[2] * m[7] +
		m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] -
		m[0] * m[7] * m[10] -
		m[4] * m[2] * m[11] +
		m[4] * m[3] * m[10] +
		m[8] * m[2] * m[7] -
		m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] +
		m[0] * m[7] * m[9] +
		m[4] * m[1] * m[11] -
		m[4] * m[3] * m[9] -
		m[8] * m[1] * m[7] +
		m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] -
		m[0] * m[6] * m[9] -
		m[4] * m[1] * m[10] +
		m[4] * m[2] * m[9] +
		m[8] * m[1] * m[6] -
		m[8] * m[2] * m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	if (det == 0.0f)
		return false;

	det = 1.0f / det;

	for (i = 0; i < 16; i++)
		invOut[i] = inv[i] * det;

	return true;
}

void transpose44(float matrix[4][4], float matrix2[4][4])
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			matrix2[j][i] = matrix[i][j];
		}
	}
}

void transpose34to44(float mat34[3][4], float mat44[4][4])
{
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			mat44[j][i] = mat34[i][j];
		}
	}
	mat44[0][3] = 0.0f;
	mat44[1][3] = 0.0f;
	mat44[2][3] = 0.0f;
	mat44[3][3] = 1.0f;
}