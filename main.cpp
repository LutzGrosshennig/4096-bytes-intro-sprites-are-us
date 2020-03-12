// We want a tiny executable, hence we omit the standard LIBC, however VC6 requires this trick in order to activate floating point
// operations. Its odd I know...
#ifndef _DEBUG 
#if defined(__cplusplus)
extern "C" {
#endif
int _fltused;
#if defined(__cplusplus)
};
#endif
#endif

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
#include <windows.h>
#include <d3dx9.h>

// Our embedded MIDI data
#include "Mucke.h"

// Our embedded sprite data
#include "Sprite.h"

// These preprocessor settings are nessary to prevent the linker from generating a bloated executable with multiple sections (each sections cost valuable space!)
#pragma optimize("gsy",on)
#pragma comment(linker,"/RELEASE")
#pragma comment(linker,"/merge:.rdata=.data")
#pragma comment(linker,"/merge:.text=.data")
#pragma comment(linker,"/merge:.reloc=.data")
#pragma comment(linker,"/FILEALIGN:0x200")

////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////
const float ZERO = 0.0f;
const float ONE  = 1.0f;

const int WIDTH  = 800;
const int HEIGHT = 600;

const int MAX_PARTICLES = 100000;

////////////////////////////////////////////////////////////////
// Data structures
////////////////////////////////////////////////////////////////
struct Vector3D
{
	float x;
	float y;
	float z;
};

struct PointSprite3D
{
	Vector3D position;
	D3DCOLOR color;
};

struct AnimatedPointSprite3D
{
	PointSprite3D sprite;
	Vector3D velocity;
};

////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////
LPDIRECT3D9 global_D3DPtr = NULL;
LPDIRECT3DDEVICE9 global_D3DDevicePtr = NULL;
LPDIRECT3DTEXTURE9 global_D3DTexturePtr = NULL;
LPDIRECT3DVERTEXBUFFER9 global_D3DVertexBufferPtr = NULL;
D3DMATRIX* global_D3DIdentityMatrix = NULL; 
AnimatedPointSprite3D* global_SpritesPtr = NULL;
BYTE global_currentEffect = 0;

///////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////
DWORD FtoDW(const float f)
{
	return *((DWORD*)&f);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
float DwToFloat(const unsigned int number)
{
	return (float)number;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void* allocMem(const int bytes)
{
	//We are allocating memory but never free it (clean up requires to much space)
	return GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, bytes);
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void calculateSinusAndCosinus(float& sinus, float& cosinus, float angle)
{
	angle = D3DXToRadian(angle);

	__asm

	{
		fld angle
		fsincos
		mov eax, cosinus
		mov ebx, sinus
		fstp DWORD PTR [EAX]
		fstp DWORD PTR [EBX]
	}
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
BYTE getRandomByte()
{
	// We are going to use the real time stamp counter to generate some fake random numbers.
	BYTE result;

	__asm
	{
		cpuid
		rdtsc
		xor al, ah
		mov result, al

		// Waste some cylces to improve the 'randomes' of the 'random' numbers.
		L :
		and eax, 0x0000003f
			dec eax
			jnz short L
	};

	return result;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
float getRandomFloat()
{
	return ((float)getRandomByte()) / 256.0f;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
BOOL getRandomBoolean()
{
	BYTE byte = getRandomByte();

	if (byte >= 128)
	{
		return TRUE;
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
float getRandomSignedFloat()
{
	float result = getRandomFloat();

	if (getRandomBoolean() == FALSE)
	{
		result = -result;
	}

	return result;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
float cotangens(const float angle)
{
	float result; //= (float)(cos(angle) / sin(angle));

	__asm
	{
		fld	angle
		fsincos
		fdiv st(0), st(1)
		fstp st(1)
		fstp result
	}

	return result;
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void setViewPort(const int x, const int y, const int width, const int height, const float minZ, const float maxZ)
{
	static D3DVIEWPORT9* vp = (D3DVIEWPORT9*)allocMem(sizeof(D3DVIEWPORT9));

	vp->X = x;
	vp->Y = y;
	vp->Width = width;
	vp->Height = height;
	vp->MinZ = minZ;
	vp->MaxZ = maxZ;
	global_D3DDevicePtr->SetViewport(vp);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void setupPerspective(const float fovY, const float aspectRatio, const float zNear, const float zFar)
{
	static D3DMATRIX* perspectiveMatrix = (D3DMATRIX*)allocMem(sizeof(D3DMATRIX));

	float h = cotangens(fovY * 0.5f);
	float w = h * aspectRatio;
	float d = (zFar - zNear);
	float a = zFar / d;
	float b = (-zNear) * zFar / d;

	perspectiveMatrix->m[0][0] = w;
	perspectiveMatrix->m[1][1] = h;
	perspectiveMatrix->m[2][2] = a;
	perspectiveMatrix->m[3][2] = b;
	perspectiveMatrix->m[2][3] = ONE;

	global_D3DDevicePtr->MultiplyTransform(D3DTS_PROJECTION, perspectiveMatrix);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void setIdentityViewMatrix()
{
	global_D3DDevicePtr->SetTransform(D3DTS_VIEW, global_D3DIdentityMatrix);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void scaleViewMatrix(float x, float y, float z)
{
	static D3DMATRIX* scaleMatrix = (D3DMATRIX*)allocMem(sizeof(D3DMATRIX));

	scaleMatrix->m[3][3] = ONE;
	scaleMatrix->m[0][0] = x;
	scaleMatrix->m[1][1] = y;
	scaleMatrix->m[2][2] = z;

	global_D3DDevicePtr->MultiplyTransform(D3DTS_VIEW, scaleMatrix);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void translateViewMatrix(const float x, const float y, const float z)
{
	static D3DMATRIX* translateMatrix = (D3DMATRIX*)allocMem(sizeof(D3DMATRIX));

	translateMatrix->m[0][0] = translateMatrix->m[1][1] = translateMatrix->m[2][2] = translateMatrix->m[3][3] = ONE;
	translateMatrix->m[3][0] = x;
	translateMatrix->m[3][1] = y;
	translateMatrix->m[3][2] = z;

	global_D3DDevicePtr->MultiplyTransform(D3DTS_VIEW, translateMatrix);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void rotateViewMatrix_Z(const float angle)
{
	static D3DMATRIX* rotatationMatrix = (D3DMATRIX*)allocMem(sizeof(D3DMATRIX));

	float sinus;
	float cosinus;

	calculateSinusAndCosinus(sinus, cosinus, angle);

	rotatationMatrix->m[2][2] = rotatationMatrix->m[3][3] = ONE;
	rotatationMatrix->m[0][0] = rotatationMatrix->m[1][1] = cosinus;
	rotatationMatrix->m[0][1] = sinus;
	rotatationMatrix->m[1][0] = -sinus;

	global_D3DDevicePtr->MultiplyTransform(D3DTS_VIEW, rotatationMatrix);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void rotateViewMatrix_X(const float angle)
{
	static D3DMATRIX* rotationMatrix = (D3DMATRIX*)allocMem(sizeof(D3DMATRIX));

	float sinus;
	float cosinus;

	calculateSinusAndCosinus(sinus, cosinus, angle);

	rotationMatrix->m[0][0] = rotationMatrix->m[3][3] = ONE;
	rotationMatrix->m[1][1] = rotationMatrix->m[2][2] = cosinus;
	rotationMatrix->m[1][2] = sinus;
	rotationMatrix->m[2][1] = -sinus;

	global_D3DDevicePtr->MultiplyTransform(D3DTS_VIEW, rotationMatrix);
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void rotateViewMatrix_Y(const float angle)
{
	static D3DMATRIX* rotationMatrix = (D3DMATRIX*)allocMem(sizeof(D3DMATRIX));

	float sinus;
	float cosinus;

	calculateSinusAndCosinus(sinus, cosinus, angle);

	rotationMatrix->m[1][1] = rotationMatrix->m[3][3] = ONE;
	rotationMatrix->m[0][0] = rotationMatrix->m[2][2] = cosinus;
	rotationMatrix->m[2][0] = sinus;
	rotationMatrix->m[0][2] = -sinus;

	global_D3DDevicePtr->MultiplyTransform(D3DTS_VIEW, rotationMatrix);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void starfield(const float delta)
{
	static float destAngleZ = (float)getRandomByte() * 0.5f;
	static float angleZ = ZERO;

	static float destAngleX = (float)getRandomByte() * 0.05f;
	static float angleX = ZERO;

	static BOOL enableXTilt = FALSE;

	static float runtime = ZERO;

	runtime += delta;

	if (runtime >= 75.0f)
	{
		++global_currentEffect;
	}

	for (int i = 0; i < MAX_PARTICLES; ++i)
	{
		global_SpritesPtr[i].sprite.position.x += global_SpritesPtr[i].velocity.x * (float)delta;
		global_SpritesPtr[i].sprite.position.y += global_SpritesPtr[i].velocity.y * (float)delta;
		global_SpritesPtr[i].sprite.position.z += global_SpritesPtr[i].velocity.z * (float)delta;

		if (global_SpritesPtr[i].sprite.position.z <= ONE)
		{
			global_SpritesPtr[i].sprite.position.z = 750.0f;
			global_SpritesPtr[i].sprite.position.x = ZERO;
			global_SpritesPtr[i].sprite.position.y = ZERO;

			enableXTilt = TRUE;
		}
	}

	angleZ += (destAngleZ - angleZ) * (float)delta;

	setIdentityViewMatrix();

	rotateViewMatrix_Z(angleZ);

	if (angleZ >= (destAngleZ - 0.1f))
	{
		float angle = (float)getRandomSignedFloat() * 64.0f;
		destAngleZ += angle;
	}

	if (enableXTilt == TRUE)
	{
		angleX += (destAngleX - angleX) * (float)delta;
		rotateViewMatrix_X(angleX);

		if (angleX >= (destAngleX - 0.05f))
		{
			float angle = (float)getRandomSignedFloat() * 2.5f;
			destAngleX += angle;
		}
	}
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void staticMorph(const float delta)
{
	static float runtime = ZERO;

	runtime += delta;

	if (runtime >= 5.0f)
	{
		++global_currentEffect;

		for (int i = 0; i < MAX_PARTICLES; ++i)
		{
			global_SpritesPtr[i].velocity.z = 20.0f;
		}
	}
	else
	{
		for (int i = 0; i < MAX_PARTICLES; ++i)
		{
			global_SpritesPtr[i].sprite.position.x += ((global_SpritesPtr[i].velocity.x - global_SpritesPtr[i].sprite.position.x) * (float)delta) * 0.5f;
			global_SpritesPtr[i].sprite.position.y += ((global_SpritesPtr[i].velocity.y - global_SpritesPtr[i].sprite.position.y) * (float)delta) * 0.5f;
			global_SpritesPtr[i].sprite.position.z += ((global_SpritesPtr[i].velocity.z - global_SpritesPtr[i].sprite.position.z) * (float)delta) * 0.5f;
		}
	}
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void staticWall(const float delta)
{
	static float runtime = ZERO;

	runtime += delta;

	if (runtime >= ONE)
	{
		++global_currentEffect;
	}
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void rotocube(const float delta)
{
	static float rotX = ZERO;
	static float rotY = ZERO;
	static float rotZ = ZERO;

	setIdentityViewMatrix();

	if (rotX >= 720.0f)
	{
		++global_currentEffect;
	}
	else
	{
		rotateViewMatrix_X(rotX);
		rotateViewMatrix_Y(rotY);
		rotateViewMatrix_Z(rotZ);

		rotX += ONE;
		rotY += 2.0f;
		rotZ += 0.5f;
	}
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void cube(const float delta)
{
	static float rotZ = ZERO;
	static float transZ = ZERO;

	setIdentityViewMatrix();

	if (rotZ >= 360.0f)
	{
		++global_currentEffect;
	}

	setIdentityViewMatrix();
	translateViewMatrix(ZERO, ZERO, -transZ);
	rotateViewMatrix_Z(rotZ);

	rotZ += ONE;
	transZ += 0.333f;

}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void morphtostarfield(const float delta)
{
	static float waittimer = ZERO;

	waittimer += delta;

	if (waittimer >= 5.0f)
	{
		++global_currentEffect;
		for (int i = 0; i < MAX_PARTICLES; ++i)
		{
			global_SpritesPtr[i].sprite.position.x = ZERO;
			global_SpritesPtr[i].sprite.position.y = ZERO;
			global_SpritesPtr[i].sprite.position.z = 750.0f;

			global_SpritesPtr[i].velocity.x = getRandomSignedFloat() * 3.5f;
			global_SpritesPtr[i].velocity.y = getRandomSignedFloat() * 2.5f;
			global_SpritesPtr[i].velocity.z = -getRandomFloat() * 100.0f;

			global_SpritesPtr[i].sprite.color = D3DCOLOR_ARGB(0, getRandomByte(), getRandomByte(), getRandomByte());
		}
	}
	else
	{
		for (int i = 0; i < MAX_PARTICLES; ++i)
		{
			global_SpritesPtr[i].sprite.position.x += (ZERO - global_SpritesPtr[i].sprite.position.x) * (float)delta;
			global_SpritesPtr[i].sprite.position.y += (ZERO - global_SpritesPtr[i].sprite.position.y) * (float)delta;
			global_SpritesPtr[i].sprite.position.z += (750.0f - global_SpritesPtr[i].sprite.position.z) * (float)delta;
		}
	}
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void morphcube(const float delta)
{
	static float rotZ = ZERO;

	if (rotZ >= 720.0f)
	{
		++global_currentEffect;

		for (int i = 0; i < MAX_PARTICLES; ++i)
		{
			global_SpritesPtr[i].velocity.z = ZERO;
		}
	}
	else
	{
		for (int i = 0; i < MAX_PARTICLES; ++i)
		{
			global_SpritesPtr[i].sprite.position.z += global_SpritesPtr[i].velocity.z * (float)delta;
		}

		setIdentityViewMatrix();
		rotateViewMatrix_Z(rotZ);
		rotZ += ONE;
	}
}


////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void update()
{
	//Lets do some "time independ" interpolation
	static float dStartAppTime = (float)timeGetTime();
	static float dLastFrameTime = dStartAppTime;

	float fElpasedAppTime = (float)((timeGetTime() - dStartAppTime) * 0.001f);
	float dCurrenFrameTime = (float)timeGetTime();
	float dElpasedFrameTime = (float)((dCurrenFrameTime - dLastFrameTime) * 0.001f);

	dLastFrameTime = dCurrenFrameTime;

	switch (global_currentEffect)
	{
		case 0: staticWall(dElpasedFrameTime); break;
		case 1: staticMorph(dElpasedFrameTime); break;
		case 2: rotocube(dElpasedFrameTime); break;
		case 3: morphcube(dElpasedFrameTime); break;
		case 4: cube(dElpasedFrameTime); break;
		case 5: morphtostarfield(dElpasedFrameTime); break;
		case 6: starfield(dElpasedFrameTime); break;
		default: break;
	};
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
void render()
{
	// Clear the screen
	global_D3DDevicePtr->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), ONE, 0);
	global_D3DDevicePtr->BeginScene();

	// Lets keep things rolling
	update();

	PointSprite3D* pPointVertices = NULL;

	// Lock the vertex buffer to get to the pointer...
	global_D3DVertexBufferPtr->Lock(0, MAX_PARTICLES * sizeof(PointSprite3D), (void**)&pPointVertices, D3DLOCK_DISCARD);

	for (int i = 0; i < MAX_PARTICLES; ++i)
	{
		*pPointVertices++ = global_SpritesPtr[i].sprite;
	}

	global_D3DVertexBufferPtr->Unlock();

	// Get things on the screen
	global_D3DDevicePtr->SetStreamSource(0, global_D3DVertexBufferPtr, 0, sizeof(PointSprite3D));
	global_D3DDevicePtr->DrawPrimitive(D3DPT_POINTLIST, 0, MAX_PARTICLES);
	global_D3DDevicePtr->EndScene();
	global_D3DDevicePtr->Present(NULL, NULL, NULL, NULL);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
#ifdef _DEBUG
void main(void)
#else
void mainCRTStartup(void)
#endif
{
	HANDLE			fileHandle;
	DWORD			numberOfBytesWritten;
	D3DDISPLAYMODE	d3ddm;

	// Hey this is MS Windows so lets create a window!
	HWND windowHandle = CreateWindowEx(WS_EX_TOPMOST,
		"STATIC",
		0,
		WS_POPUP | WS_VISIBLE,
		0, 0,
		WIDTH, HEIGHT,
		0, 0, 0, 0);

	// Hide the cursor
	ShowCursor(FALSE);

	// We will play the embedded MIDI using the MCI Interface, simple and small
	// First we need to store the embedded data to disc.
	fileHandle = CreateFile("C:\\S.mid",
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		NULL);

	WriteFile(fileHandle, (LPCVOID)&Mucke, sizeof(Mucke), &numberOfBytesWritten, NULL);
	CloseHandle(fileHandle);

	// Setup DirectX
	global_D3DPtr = Direct3DCreate9(D3D_SDK_VERSION);
	global_D3DPtr->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);

	D3DPRESENT_PARAMETERS* d3dpp = (D3DPRESENT_PARAMETERS*)allocMem(sizeof(D3DPRESENT_PARAMETERS));

	d3dpp->BackBufferWidth = WIDTH;
	d3dpp->BackBufferHeight = HEIGHT;

#ifdef _DEBUG
	d3dpp->Windowed = TRUE;
#else
	d3dpp->Windowed = FALSE;
#endif

	d3dpp->SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp->BackBufferFormat = d3ddm.Format;
//	d3dpp->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
	d3dpp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	global_D3DPtr->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, windowHandle,
		D3DCREATE_HARDWARE_VERTEXPROCESSING, d3dpp, &global_D3DDevicePtr);

	global_D3DDevicePtr->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	global_D3DDevicePtr->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	global_D3DDevicePtr->SetRenderState(D3DRS_ZENABLE, FALSE);
	global_D3DDevicePtr->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	global_D3DDevicePtr->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	global_D3DDevicePtr->SetRenderState(D3DRS_LIGHTING, FALSE);

	global_D3DDevicePtr->SetRenderState(D3DRS_POINTSPRITEENABLE, TRUE);
	global_D3DDevicePtr->SetRenderState(D3DRS_POINTSCALEENABLE, TRUE);
	global_D3DDevicePtr->SetRenderState(D3DRS_POINTSCALE_A, FtoDW(ZERO));
	global_D3DDevicePtr->SetRenderState(D3DRS_POINTSCALE_B, FtoDW(ZERO));
	global_D3DDevicePtr->SetRenderState(D3DRS_POINTSCALE_C, FtoDW(ONE));

	global_D3DDevicePtr->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);

	// Setup the identity view matrix.
	global_D3DIdentityMatrix = (D3DMATRIX*)allocMem(sizeof(D3DMATRIX));
	global_D3DIdentityMatrix->m[0][0] = global_D3DIdentityMatrix->m[1][1] = global_D3DIdentityMatrix->m[2][2] = global_D3DIdentityMatrix->m[3][3] = ONE;

	// Setup a perspective projection.
	setupPerspective(D3DX_PI / 4.0f, 4.0f / 3.0f, ONE, 1000.0f);

	// The vertex buffer containing all the sprite cooridinates
	global_D3DDevicePtr->CreateVertexBuffer(MAX_PARTICLES * sizeof(PointSprite3D),
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY | D3DUSAGE_POINTS,
		D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DPOOL_DEFAULT,
		&global_D3DVertexBufferPtr, NULL);

	// Our sprite data only contains one color channel so we duplicate the data to all four channels (including alpha).
	global_D3DDevicePtr->CreateTexture(8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &global_D3DTexturePtr, NULL);

	D3DLOCKED_RECT* rectangleLock = (D3DLOCKED_RECT*)allocMem(sizeof(D3DLOCKED_RECT));

	if (global_D3DTexturePtr->LockRect(0, rectangleLock, 0, 0) == D3D_OK)
	{
		unsigned char* pixelData = (unsigned char*)rectangleLock->pBits;

		int counter = 0;

		for (int y = 0; y < 8; ++y)
		{
			BYTE* rowData = pixelData;

			for (int x = 0; x < 8; ++x)
			{
				rowData[0] = rowData[1] = rowData[2] = rowData[3] = SpriteLayer[counter];
				++counter;
				rowData += 4;
			}

			pixelData += rectangleLock->Pitch;
		}
		global_D3DTexturePtr->UnlockRect(0);
	}

	global_D3DDevicePtr->SetTexture(0, global_D3DTexturePtr);

	// Lets generate our sprites!
	global_SpritesPtr = (AnimatedPointSprite3D*)allocMem(MAX_PARTICLES * sizeof(AnimatedPointSprite3D));

	unsigned int i = 0;

	for (int z = 0; z < 10; ++z)
	{
		for (int y = 0; y < 100; ++y)
		{
			for (int x = 0; x < 100; ++x)
			{
				global_SpritesPtr[i].sprite.position.x = (float)(x - 50) * 0.025f;
				global_SpritesPtr[i].sprite.position.y = (float)(y - 50) * 0.025f;
				global_SpritesPtr[i].sprite.position.z = (float)(z - 5);

				global_SpritesPtr[i].sprite.color = D3DCOLOR_ARGB(0, getRandomByte(), getRandomByte(), getRandomByte());
			
				global_SpritesPtr[i].velocity.x = (float)(x - 50) * 0.5f;
				global_SpritesPtr[i].velocity.y = (float)(y - 50) * 0.5f;
				global_SpritesPtr[i].velocity.z = (float)(z - 5);
				++i;
			}
		}	
	}

	// Now we can use the MCI to play back that file.
	mciSendString("open C:\\S.mid type sequencer alias mucke", 0, 0, 0);
	mciSendString("play mucke from 0", 0, 0, 0);

	// Main loop
	while (global_currentEffect < 7)
    {
		MSG msg;

		// Lets pretend that we are listening to Windows messages.
		PeekMessage(&msg, windowHandle, 0, 0, PM_REMOVE); 
		
		// Draw stuff.
		render();
    }

	// Clean up time MCI.
	mciSendString("stop mucke", 0, 0, 0);
	mciSendString("close mucke", 0, 0, 0);

	// Clean up D3D.
	global_D3DDevicePtr->Release();
	global_D3DPtr->Release();

	//Clean up the temporary file.
	DeleteFile("C:\\S.mid");

// When building as "release" we use wincrtStartup as entrypoint so we need to explicty kill the process.
#ifndef _DEBUG
	ExitProcess(0);
#endif

	// Note: we never free any memory deliberatly. It takes valueable space and the process will terminate 
}
