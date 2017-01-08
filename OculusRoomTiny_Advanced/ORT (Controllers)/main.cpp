/************************************************************************************
Filename    :   Win32_RoomTiny_Main.cpp
Content     :   First-person view test application for Oculus Rift
Created     :   30th July 2015
Authors     :   Tom Heath
Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/
/// An initial simple sample to show how to interrogate the touch
/// controllers.  In this sample, just the left controller is dealt with.
/// Move and rotate the controller, and also change its colour
/// by holding the X and Y buttons. 

#include "../Common/Win32_DirectXAppUtil.h" // DirectX
#include "../Common/Win32_BasicVR.h"        // Basic VR
#include "DisplayManager.h"
#include "DuplicationManager.h"
#include "OutputManager.h"
#include "ThreadManager.h"
#include "..\..\DepthWithColor-D3D\DepthWithColor-D3D.h"
#pragma comment(lib,"user32.lib") 
#pragma comment(lib,"gdi32.lib") 
struct Controllers : BasicVR
{
    Controllers(HINSTANCE hinst) : BasicVR(hinst, L"Controllers") {}

	/* Globals */
	int ScreenX = 0;
	int ScreenY = 0;
	BYTE* ScreenData = 0;
	HDC hScreen;
	HDC hdcMem;
	HBITMAP hBitmap;
	HGDIOBJ hOld;

	void InitCap() {
		hScreen = GetDC(GetDesktopWindow());
		ScreenX = GetDeviceCaps(hScreen, HORZRES);
		ScreenY = GetDeviceCaps(hScreen, VERTRES);
		hdcMem = CreateCompatibleDC(hScreen);
		hBitmap = CreateCompatibleBitmap(hScreen, ScreenX, ScreenY);
		hOld = SelectObject(hdcMem, hBitmap);
		ScreenData = (BYTE*)malloc(4 * ScreenX * ScreenY);
		Cleanup();
	}

	void ScreenCap(Texture* tex)
	{
		hScreen = GetDC(GetDesktopWindow());
		hdcMem = CreateCompatibleDC(hScreen);
		hBitmap = CreateCompatibleBitmap(hScreen, ScreenX, ScreenY);
		hOld = SelectObject(hdcMem, hBitmap);

		BitBlt(hdcMem, 0, 0, ScreenX, ScreenY, hScreen, 0, 0, SRCCOPY);
		SelectObject(hdcMem, hOld);

		BITMAPINFOHEADER bmi = { 0 };
		bmi.biSize = sizeof(BITMAPINFOHEADER);
		bmi.biPlanes = 1;
		bmi.biBitCount = 32;
		bmi.biWidth = ScreenX;
		bmi.biHeight = -ScreenY;
		bmi.biCompression = BI_RGB;
		bmi.biSizeImage = 0;// 3 * ScreenX * ScreenY;

		GetDIBits(hdcMem, hBitmap, 0, ScreenY, ScreenData, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

		Cleanup();

		tex->FillTexture((uint32_t *)ScreenData, true);
	}

	void Cleanup() 
	{
		ReleaseDC(GetDesktopWindow(), hScreen);
		DeleteDC(hdcMem);
		DeleteObject(hBitmap);
	}

	inline int PosB(int x, int y)
	{
		return ScreenData[4 * ((y*ScreenX) + x)];
	}

	inline int PosG(int x, int y)
	{
		return ScreenData[4 * ((y*ScreenX) + x) + 1];
	}

	inline int PosR(int x, int y)
	{
		return ScreenData[4 * ((y*ScreenX) + x) + 2];
	}

    void MainLoop()
    {		

	    Layer[0] = new VRLayer(Session);

		// Synchronization
		HANDLE UnexpectedErrorEvent = nullptr;
		HANDLE ExpectedErrorEvent = nullptr;
		HANDLE TerminateThreadsEvent = nullptr;

		CDepthWithColorD3D g_Application(DIRECTX.Device, DIRECTX.Context);

		if (FAILED(g_Application.CreateFirstConnected()))
		{
			ProcessFailure(NULL, L"No ready Kinect found!", L"Error", MB_ICONHAND | MB_OK);
			return;
		}
		g_Application.InitDevice();

		// Event used by the threads to signal an unexpected error and we want to quit the app
		UnexpectedErrorEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		if (!UnexpectedErrorEvent)
		{
			ProcessFailure(nullptr, L"UnexpectedErrorEvent creation failed", L"Error", E_UNEXPECTED);
			return;
		}

		// Event for when a thread encounters an expected error
		ExpectedErrorEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		if (!ExpectedErrorEvent)
		{
			ProcessFailure(nullptr, L"ExpectedErrorEvent creation failed", L"Error", E_UNEXPECTED);
			return;
		}

		// Event to tell spawned threads to quit
		TerminateThreadsEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		if (!TerminateThreadsEvent)
		{
			ProcessFailure(nullptr, L"TerminateThreadsEvent creation failed", L"Error", E_UNEXPECTED);
			return;
		}
		
		// Create a trivial model to represent the left controller
	    TriangleSet cube;
	    cube.AddSolidColorBox(0.05f, -0.05f, 0.05f, -0.05f, 0.05f, -0.05f, 0xff404040);
	    Model * controller = new Model(&cube, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1), new Material(new Texture(false, 256, 256, Texture::AUTO_CEILING)));
	
		InitCap();
		Texture* tex = new Texture(ScreenX, ScreenY, true);
		TriangleSet wall;
		//wall.AddSolidColorBox(1.0f, -1.0f, 1.0f, -0.05f, 0.05f, -0.05f, 0xff404040);
		float x1 = -3840.0f / 2160.0f / 2,
			y1 = 0.5f, 
			z1 = 1.0f, 
			x2 = 3840.0f / 2160.0f / 2, 
			y2 = -0.5f;// , z2 = -0.05f;

		uint32_t c = 0xFFFFFFFF;
		wall.AddQuad(
			Vertex(XMFLOAT3(x1, y1, z1), c, 0, 0),
			Vertex(XMFLOAT3(x2, y1, z1), c, 1, 0),
			Vertex(XMFLOAT3(x1, y2, z1), c, 0, 1),
			Vertex(XMFLOAT3(x2, y2, z1), c, 1, 1)
		);
		wall.AddQuad(
			Vertex(XMFLOAT3(x2, y1, z1), c, 1, 0),
			Vertex(XMFLOAT3(x1, y1, z1), c, 0, 0),
			Vertex(XMFLOAT3(x2, y2, z1), c, 1, 1),
			Vertex(XMFLOAT3(x1, y2, z1), c, 0, 1)
		);
		Model * thing = new Model(&wall, XMFLOAT3(-2.5, 1.0, 0), XMFLOAT4(0, 0, 0, 1), new Material(tex));

		// Main loop
		//int frame = 0;
		THREADMANAGER ThreadMgr;
		OUTPUTMANAGER OutMgr(tex->Tex);
		RECT DeskBounds;
		UINT OutputCount;
		int SingleOutput = -1;
		HWND WindowHandle = 0;
		bool Occluded = true;

		// Message loop (attempts to update screen when no other messages to process)
		MSG msg = { 0 };
		bool FirstTime = true;

		static float Yaw = -2;
		MainCam->Rot = XMQuaternionRotationRollPitchYaw(0, Yaw, 0);

		while (WM_QUIT != msg.message)
		{
			DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				if (msg.message == OCCLUSION_STATUS_MSG)
				{
					// Present may not be occluded now so try again
					Occluded = false;
				}
				else
				{
					// Process window messages
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			else if (WaitForSingleObjectEx(UnexpectedErrorEvent, 0, FALSE) == WAIT_OBJECT_0)
			{
				// Unexpected error occurred so exit the application
				break;
			}
			else if (FirstTime || WaitForSingleObjectEx(ExpectedErrorEvent, 0, FALSE) == WAIT_OBJECT_0)
			{
				if (!FirstTime)
				{
					// Terminate other threads
					SetEvent(TerminateThreadsEvent);
					ThreadMgr.WaitForThreadTermination();
					ResetEvent(TerminateThreadsEvent);
					ResetEvent(ExpectedErrorEvent);

					// Clean up
					ThreadMgr.Clean();
					OutMgr.CleanRefs();

					// As we have encountered an error due to a system transition we wait before trying again, using this dynamic wait
					// the wait periods will get progressively long to avoid wasting too much system resource if this state lasts a long time
					//DynamicWait.Wait();
				}
				else
				{
					// First time through the loop so nothing to clean up
					FirstTime = false;
				}

				// Re-initialize
				Ret = OutMgr.InitOutput(WindowHandle, SingleOutput, &OutputCount, &DeskBounds);
				if (Ret == DUPL_RETURN_SUCCESS)
				{
					HANDLE SharedHandle = OutMgr.GetSharedHandle();
					if (SharedHandle)
					{
						Ret = ThreadMgr.Initialize(SingleOutput, OutputCount, UnexpectedErrorEvent, ExpectedErrorEvent, TerminateThreadsEvent, SharedHandle, &DeskBounds);
					}
					else
					{
						DisplayMsg(L"Failed to get handle of shared surface", L"Error", S_OK);
						Ret = DUPL_RETURN_ERROR_UNEXPECTED;
					}
				}

				// We start off in occluded state and we should immediate get a occlusion status window message
				Occluded = true;
			}
			else
			{
				// Nothing else to do, so try to present to write out to window if not occluded
				Ret = OutMgr.UpdateApplicationWindow(ThreadMgr.GetPointerInfo(), &Occluded);
			}

			// Check if for errors
			if (Ret != DUPL_RETURN_SUCCESS)
			{
				if (Ret == DUPL_RETURN_ERROR_EXPECTED)
				{
					// Some type of system transition is occurring so retry
					SetEvent(ExpectedErrorEvent);
				}
				else
				{
					// Unexpected error so exit
					break;
				}
			}
			
			// We don't allow yaw change for now, as this sample is too simple to cater for it.
			ActionFromInput(1.0f, false, true);
			ovrTrackingState hmdState = Layer[0]->GetEyePoses();

			//Write position and orientation into controller model.
			controller->Pos = XMFLOAT3(XMVectorGetX(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.x * 20,
				XMVectorGetY(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.y,
				XMVectorGetZ(MainCam->Pos) + hmdState.HandPoses[ovrHand_Left].ThePose.Position.z);
			controller->Rot = XMFLOAT4(hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.x,
				hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.y,
				hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.z,
				hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.w);
				
			//Button presses are modifying the colour of the controller model below
			ovrInputState inputState;
			ovr_GetInputState(Session, ovrControllerType_XBox, &inputState);
			
			if (inputState.Thumbstick[1].x < -.5)   MainCam->Rot = XMQuaternionRotationRollPitchYaw(0, Yaw += 0.02f, 0);
			if (inputState.Thumbstick[1].x > .5)    MainCam->Rot = XMQuaternionRotationRollPitchYaw(0, Yaw -= 0.02f, 0);


			XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -0.05f, 0), MainCam->Rot);
			XMVECTOR right = XMVector3Rotate(XMVectorSet(0.05f, 0, 0, 0), MainCam->Rot);
			XMVECTOR up = XMVector3Rotate(XMVectorSet(0, 0.05f, 0, 0), MainCam->Rot);
			//XMVECTOR forward = XMVectorSet(0, 0, -0.05f, 0);
			//XMVECTOR right = XMVectorSet(0.05f, 0, 0, 0);

			if (inputState.Thumbstick[1].y > -.5)   MainCam->Pos = XMVectorAdd(MainCam->Pos, up);
			if (inputState.Thumbstick[1].y < .5)    MainCam->Pos = XMVectorSubtract(MainCam->Pos, up);
			if (inputState.Thumbstick->y > .5)	  MainCam->Pos = XMVectorAdd(MainCam->Pos, forward);
			if (inputState.Thumbstick->y < -.5)    MainCam->Pos = XMVectorSubtract(MainCam->Pos, forward);
			if (inputState.Thumbstick->x > -.5)    MainCam->Pos = XMVectorAdd(MainCam->Pos, right);
			if (inputState.Thumbstick->x < .5)    MainCam->Pos = XMVectorSubtract(MainCam->Pos, right);

			//ovr_GetInputState(Session, ovrControllerType_Touch, &inputState);
			for (int eye = 0; eye < 2; ++eye)
			{
				XMMATRIX viewProj = Layer[0]->RenderSceneToEyeBuffer(MainCam, RoomScene, eye);

				g_Application.Render(DIRECTX.Context, viewProj);

				// Render the controller model
				controller->Render(&viewProj, 1, inputState.Buttons & ovrTouch_X ? 1.0f : 0.0f,
					inputState.Buttons & ovrTouch_Y ? 1.0f : 0.0f, 1, true);

				//if (frame++ % 1000 == 0)
					//ScreenCap(tex);
				thing->Render(&viewProj, 1.0f, 1.0f, 1.0f, 1.0f, true);
			}

			Layer[0]->PrepareLayerHeader();
			DistortAndPresent(1);
		}

		// Make sure all other threads have exited
		if (SetEvent(TerminateThreadsEvent))
		{
			ThreadMgr.WaitForThreadTermination();
		}

		// Clean up
		CloseHandle(UnexpectedErrorEvent);
		CloseHandle(ExpectedErrorEvent);
		CloseHandle(TerminateThreadsEvent);

		if (msg.message == WM_QUIT)
		{
			// For a WM_QUIT message we should return the wParam value
			return;
		}

        delete controller;
    }
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	Controllers app(hinst);
    return app.Run();
}
