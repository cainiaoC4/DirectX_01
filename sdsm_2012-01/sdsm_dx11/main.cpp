// Copyright 2012 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "App.h"
#include "Partitions.h"
#include <sstream>


// Constants
static const float kLightRotationSpeed = 0.05f;
static const float kSliderFactorResolution = 10000.0f;

enum SCENE_SELECTION {
    POWER_PLANT_SCENE,
    TOWER_SCENE,
};

enum {
    UI_TOGGLEFULLSCREEN,
    UI_TOGGLEWARP,
    UI_CHANGEDEVICE,
    UI_ANIMATELIGHT,
    UI_VISUALIZEPARTITIONS,
    UI_VISUALIZEHISTOGRAM,
    UI_VISUALIZEERROR,
    UI_FACENORMALS,
    UI_SELECTEDSCENE,
    UI_PARTITIONSCHEME,
    UI_PARTITIONS,
    UI_PARTITIONSPERPASS,
    UI_SHADOWTEXTUREDIM,
    UI_FILTERINGSCHEME,
    UI_SHADOWAA,
    UI_EDGESOFTENING,
    UI_EDGESOFTENINGAMOUNT,
    UI_USEPOSITIVEEXPONENT,
    UI_POSITIVEEXPONENT,
    UI_USENEGATIVEEXPONENT,
    UI_NEGATIVEEXPONENT,
    UI_LLOYDITERATIONSTEXT,
    UI_LLOYDITERATIONS,
    UI_HISTOGRAMRESOLUTIONPOWERTEXT,
    UI_HISTOGRAMRESOLUTIONPOWER,
    UI_PSSMFACTORTEXT,
    UI_PSSMFACTOR,
    UI_TIGHTPARTITIONBOUNDS,
    UI_ALIGNLIGHTTOFRUSTUM,
    UI_CPUPARTITIONFRUSTUMCULLING,
    UI_SHADOWTRANSPARENCYAA,
    UI_LIGHTINGONLY,
    UI_SAVENORMALIZEDDEPTH,
    UI_VISUALIZELIGHTSPACE,
	UI_QUANTIZEPARTITIONS,
};

// List these top to bottom, since it is also the reverse draw order
enum {
    HUD_GENERIC = 0,
    HUD_PARTITIONS,
    HUD_FILTERING,
    HUD_EXPERT,
    HUD_NUM,
};


App* gApp = 0;

CFirstPersonCamera gViewerCamera;
D3DXVECTOR3 gLightDirection;

CDXUTSDKMesh gMeshOpaque;
CDXUTSDKMesh gMeshAlpha;
D3DXMATRIXA16 gWorldMatrix;
ID3D11ShaderResourceView* gSkyboxSRV = 0;

// DXUT GUI stuff
CDXUTDialogResourceManager gDialogResourceManager;
CD3DSettingsDlg gD3DSettingsDlg;
CDXUTDialog gHUD[HUD_NUM];
CDXUTCheckBox* gAnimateLightCheck = 0;
CDXUTComboBox* gCameraSelectCombo = 0;
CDXUTComboBox* gSceneSelectCombo = 0;
CDXUTComboBox* gPartitionSchemeCombo = 0;
CDXUTComboBox* gPartitionsCombo = 0;
CDXUTComboBox* gPartitionsPerPassCombo = 0;
CDXUTComboBox* gShadowTextureDimCombo = 0;
CDXUTComboBox* gFilteringSchemeCombo = 0;
CDXUTComboBox* gShadowAACombo = 0;
CDXUTCheckBox* gTightPartitionBoundsCheck = 0;
CDXUTCheckBox* gQuantizePartitionsCheck = 0;
CDXUTCheckBox* gAlignLightToFrustumCheck = 0;
CDXUTCheckBox* gShadowTransparencyAACheck = 0;
CDXUTTextHelper* gTextHelper = 0;

bool gDisplayUI = true;
bool gZeroNextFrameTime = true;

// Any UI state passed directly to rendering shaders
UIConstants gUIConstants;
FilteringScheme gDefaultFiltering;
int gDefaultPartitions;
int gDefaultShadowDim;
int gDefaultShadowAASamples;

bool CALLBACK CommandLineArg(const WCHAR* arg, const WCHAR* param, void* userContext);
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* deviceSettings, void* userContext);
void CALLBACK OnFrameMove(double time, float elapsedTime, void* userContext);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* noFurtherProcessing,
                         void* userContext);
void CALLBACK OnKeyboard(UINT character, bool keyDown, bool altDown, void* userContext);
void CALLBACK OnGUIEvent(UINT eventID, INT controlID, CDXUTControl* control, void* userContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferSurfaceDesc,
                                     void* userContext);
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, IDXGISwapChain* swapChain,
                                         const DXGI_SURFACE_DESC* backBufferSurfaceDesc, void* userContext);
void CALLBACK OnD3D11ReleasingSwapChain(void* userContext);
void CALLBACK OnD3D11DestroyDevice(void* userContext);
void CALLBACK OnD3D11FrameRender(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, double time,
                                 float elapsedTime, void* userContext);

void LoadSkybox(LPCWSTR fileName);

void InitApp(ID3D11Device* d3dDevice);
void DestroyApp();
void InitScene(ID3D11Device* d3dDevice);
void DestroyScene();

void InitUI();
void UpdateUIState();


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// Setup default UI state
    // NOTE: All of these are made directly available in the shader constant buffer
    // This is convenient for development purposes.
    gUIConstants.lightingOnly = 0;
    gUIConstants.faceNormals = 1;
    gUIConstants.partitionScheme = LOG_PARTITIONS_REDUCE;
    gUIConstants.visualizePartitions = 0;
    gUIConstants.visualizeHistogram = 0;
    gUIConstants.visualizeError = 0;
    gUIConstants.edgeSoftening = 0;
    gUIConstants.edgeSofteningAmount = 0.02f;
    gUIConstants.maxEdgeSofteningFilter = 16.0f;   // Max blur kernel; EVSM can handle more but PCF can't...
    gUIConstants.pssmFactor = 0.8f;
    gUIConstants.alignLightToFrustum = 1;
    gUIConstants.cpuPartitionFrustumCulling = 1;
    gUIConstants.tightPartitionBounds = 0;
    gUIConstants.histogramResolutionPower = 10.0f;
    gUIConstants.lloydIterations = 32;
    gUIConstants.usePositiveExponent = 1;
    gUIConstants.positiveExponent = 800.0f;
    gUIConstants.useNegativeExponent = 1;
    gUIConstants.negativeExponent = 100.0f;
    gUIConstants.visualizeLightSpace = 0;
	gUIConstants.quantizePartitions = 0;

	gDefaultFiltering = EVSM_FILTERING;
	gDefaultPartitions = 4;
	gDefaultShadowDim = 1024;
	gDefaultShadowAASamples = 4;

    DXUTSetCallbackCommandLineArg(CommandLineArg);
    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackFrameMove(OnFrameMove);

    DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
    
    DXUTInit(true, true, 0);
    InitUI();

    DXUTSetCursorSettings(true, true);
    DXUTSetHotkeyHandling(true, true, false);
    DXUTCreateWindow(L"Sample Distribution Shadow Maps");
    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1280, 720);
    DXUTMainLoop();

    return DXUTGetExitCode();
}


void InitUI()
{	    
    gD3DSettingsDlg.Init(&gDialogResourceManager);

    for (int i = 0; i < HUD_NUM; ++i) {
        gHUD[i].Init(&gDialogResourceManager);
        gHUD[i].SetCallback(OnGUIEvent);
    }

    int width = 200;

    // Generic HUD
    {
        CDXUTDialog* HUD = &gHUD[HUD_GENERIC];
        int y = 0;

        HUD->AddButton(UI_TOGGLEFULLSCREEN, L"Toggle full screen", 0, y, width, 23);
        y += 26;

        // Warp doesn't support DX11 yet
        //HUD->AddButton(UI_TOGGLEWARP, L"Toggle WARP (F3)", 0, y, width, 23, VK_F3);
        //y += 26;

        HUD->AddButton(UI_CHANGEDEVICE, L"Change device (F2)", 0, y, width, 23, VK_F2);
        y += 26;

        HUD->AddComboBox(UI_SELECTEDSCENE, 0, y, width, 23, 0, false, &gSceneSelectCombo);
        y += 26;
        gSceneSelectCombo->AddItem(L"Power Plant", ULongToPtr(POWER_PLANT_SCENE));
        gSceneSelectCombo->AddItem(L"Tower", ULongToPtr(TOWER_SCENE));

        HUD->AddCheckBox(UI_ANIMATELIGHT, L"Animate Light", 0, y, width, 23, false, 0, false, &gAnimateLightCheck);
        y += 26;

        HUD->AddCheckBox(UI_LIGHTINGONLY, L"Lighting Only", 0, y, width, 23, gUIConstants.lightingOnly != 0);
        y += 26;

        HUD->AddCheckBox(UI_FACENORMALS, L"Face Normals", 0, y, width, 23, gUIConstants.faceNormals != 0);
        y += 26;
        
        HUD->SetSize(width, y);
    }

    // Partition HUD
    {
        CDXUTDialog* HUD = &gHUD[HUD_PARTITIONS];
        int y = 0;
    
        HUD->AddComboBox(UI_PARTITIONSCHEME, 0, y, width, 23, 0, false, &gPartitionSchemeCombo);
        y += 26;
        gPartitionSchemeCombo->AddItem(L"Log Partitions", ULongToPtr(LOG_PARTITIONS));
        gPartitionSchemeCombo->AddItem(L"Log Partitions (Reduce)", ULongToPtr(LOG_PARTITIONS_REDUCE));
        gPartitionSchemeCombo->AddItem(L"Adaptive Log Partitions", ULongToPtr(ADAPTIVE_LOG_PARTITIONS));
        gPartitionSchemeCombo->AddItem(L"K-Means Partitions", ULongToPtr(KMEANS_PARTITIONS));
        gPartitionSchemeCombo->AddItem(L"PSSM Partitions", ULongToPtr(PSSM_PARTITIONS));
        gPartitionSchemeCombo->SetSelectedByData(ULongToPtr(gUIConstants.partitionScheme));

        const int maxPartitions = 8;

        HUD->AddComboBox(UI_PARTITIONS, 0, y, width, 23, 0, false, &gPartitionsCombo);
        y += 26;
        HUD->AddComboBox(UI_PARTITIONSPERPASS, 0, y, width, 23, 0, false, &gPartitionsPerPassCombo);
        y += 26;
        for (int i = 1; i <= maxPartitions; ++i) {
            std::wostringstream oss;
            oss << i << " Partition";
            if (i > 1) oss << 's';
            gPartitionsCombo->AddItem(oss.str().c_str(), ULongToPtr(i));
            oss << " / Pass";
            gPartitionsPerPassCombo->AddItem(oss.str().c_str(), ULongToPtr(i));
        }
        gPartitionsCombo->SetSelectedByData(ULongToPtr(gDefaultPartitions));
        gPartitionsPerPassCombo->SetSelectedByData(ULongToPtr(1));

        HUD->AddComboBox(UI_SHADOWTEXTUREDIM, 0, y, width, 23, 0, false, &gShadowTextureDimCombo);
        y += 26;
        gShadowTextureDimCombo->AddItem(L"256 x 256", ULongToPtr(256));
        gShadowTextureDimCombo->AddItem(L"512 x 512", ULongToPtr(512));
        gShadowTextureDimCombo->AddItem(L"1024 x 1024", ULongToPtr(1024));
        gShadowTextureDimCombo->AddItem(L"2048 x 2048", ULongToPtr(2048));
        gShadowTextureDimCombo->SetSelectedByData(ULongToPtr(gDefaultShadowDim));

        HUD->AddCheckBox(UI_VISUALIZEPARTITIONS, L"Visualize Partitions", 0, y, width, 23, gUIConstants.visualizePartitions != 0);
        y += 26;

        HUD->AddCheckBox(UI_VISUALIZELIGHTSPACE, L"Visualize Light Space", 0, y, width, 23, gUIConstants.visualizeLightSpace != 0);
        y += 26;

        HUD->AddCheckBox(UI_VISUALIZEHISTOGRAM, L"Visualize Histogram", 0, y, width, 23, gUIConstants.visualizeHistogram != 0);
        y += 26;

        HUD->AddCheckBox(UI_VISUALIZEERROR, L"Visualize Error", 0, y, width, 23, gUIConstants.visualizeError != 0);
        y += 26;

        HUD->SetSize(width, y);
    }

    // Filtering HUD
    {
        CDXUTDialog* HUD = &gHUD[HUD_FILTERING];
        int y = 0;
    
        HUD->AddComboBox(UI_FILTERINGSCHEME, 0, y, width, 23, 0, false, &gFilteringSchemeCombo);
        y += 26;
        gFilteringSchemeCombo->AddItem(L"EVSM Filtering", ULongToPtr(EVSM_FILTERING));
        gFilteringSchemeCombo->AddItem(L"Neighbor PCF", ULongToPtr(NEIGHBOR_PCF_FILTERING));
        gFilteringSchemeCombo->SetSelectedByData(ULongToPtr(gDefaultFiltering));

        HUD->AddComboBox(UI_SHADOWAA, 0, y, width, 23, 0, false, &gShadowAACombo);
        y += 26;
        gShadowAACombo->AddItem(L"No Shadow AA", ULongToPtr(1));
        gShadowAACombo->AddItem(L"2x Shadow AA", ULongToPtr(2));
        gShadowAACombo->AddItem(L"4x Shadow AA", ULongToPtr(4));
        gShadowAACombo->AddItem(L"8x Shadow AA", ULongToPtr(8));
        gShadowAACombo->SetSelectedByData(ULongToPtr(gDefaultShadowAASamples));

        HUD->AddCheckBox(UI_EDGESOFTENING, L"Edge Softening", 0, y, width, 23, gUIConstants.edgeSoftening != 0);
        y += 26;

        HUD->AddSlider(UI_EDGESOFTENINGAMOUNT, 0, y, width, 23, 0,
            static_cast<int>(kSliderFactorResolution * 0.1f),       //[0, 0.1] world space units
            static_cast<int>(gUIConstants.edgeSofteningAmount * kSliderFactorResolution));
        y += 26;

        HUD->SetSize(width, y);
    }

    // Expert HUD
    {
        CDXUTDialog* HUD = &gHUD[HUD_EXPERT];
        int y = 0;
    
        HUD->AddButton(UI_SAVENORMALIZEDDEPTH, L"Save Depth", 0, y, width, 23);
        y += 26;

        HUD->AddCheckBox(UI_ALIGNLIGHTTOFRUSTUM, L"Align Light To Frustum", 0, y, width, 23, gUIConstants.alignLightToFrustum != 0, 0, false, &gAlignLightToFrustumCheck);
        y += 26;

        HUD->AddCheckBox(UI_CPUPARTITIONFRUSTUMCULLING, L"CPU Partition Culling", 0, y, width, 23, gUIConstants.cpuPartitionFrustumCulling != 0);
        y += 26;

        HUD->AddCheckBox(UI_SHADOWTRANSPARENCYAA, L"Shadow Transparency AA", 0, y, width, 23, false, 0, false, &gShadowTransparencyAACheck);
        y += 26;

		HUD->AddCheckBox(UI_QUANTIZEPARTITIONS, L"Quantize Partitions", 0, y, width, 23, gUIConstants.quantizePartitions != 0, 0, false, &gQuantizePartitionsCheck);
        y += 26;

        HUD->AddStatic(UI_PSSMFACTORTEXT, L"PSSM Factor:", 0, y, width, 23);
        y += 26;
        HUD->AddSlider(UI_PSSMFACTOR, 0, y, width, 23,
            0, static_cast<int>(kSliderFactorResolution),
            static_cast<int>(gUIConstants.pssmFactor * kSliderFactorResolution));
        y += 26;

        HUD->AddCheckBox(UI_TIGHTPARTITIONBOUNDS, L"Tight Partition Bounds", 0, y, width, 23, gUIConstants.tightPartitionBounds != 0, 0, false, &gTightPartitionBoundsCheck);
        y += 26;

        HUD->AddStatic(UI_HISTOGRAMRESOLUTIONPOWERTEXT, L"Histogram Resolution:", 0, y, width, 23);
        y += 26;
        HUD->AddSlider(UI_HISTOGRAMRESOLUTIONPOWER, 0, y, width, 23,
            static_cast<int>(kSliderFactorResolution * 1.0f),       //[1, 30] exponent
            static_cast<int>(kSliderFactorResolution * 30.0f),
            static_cast<int>(gUIConstants.histogramResolutionPower * kSliderFactorResolution));
        y += 26;

        HUD->AddStatic(UI_LLOYDITERATIONSTEXT, L"Lloyd Iterations:", 0, y, width, 23);
        y += 26;
        HUD->AddSlider(UI_LLOYDITERATIONS, 0, y, width, 23, 0, 32, gUIConstants.lloydIterations);
        y += 26;

        HUD->AddCheckBox(UI_USEPOSITIVEEXPONENT, L"Positive Exponent", 0, y, width, 23, gUIConstants.usePositiveExponent != 0);
        y += 26;
        HUD->AddSlider(UI_POSITIVEEXPONENT, 0, y, width, 23,
            static_cast<int>(kSliderFactorResolution * 0.0f),
            static_cast<int>(kSliderFactorResolution * 2000.0f),       //[0, 2000] world space units
            static_cast<int>(gUIConstants.positiveExponent * kSliderFactorResolution));
        y += 26;

        HUD->AddCheckBox(UI_USENEGATIVEEXPONENT, L"Negative Exponent", 0, y, width, 23, gUIConstants.useNegativeExponent != 0);
        y += 26;
        HUD->AddSlider(UI_NEGATIVEEXPONENT, 0, y, width, 23,
            static_cast<int>(kSliderFactorResolution * 0.0f),
            static_cast<int>(kSliderFactorResolution * 2000.0f),       //[0, 2000] world space units
            static_cast<int>(gUIConstants.negativeExponent * kSliderFactorResolution));
        y += 26;

        HUD->SetSize(width, y);

        // Initially hidden
        HUD->SetVisible(false);
    }

    UpdateUIState();
}


void UpdateUIState()
{
    FilteringScheme filteringScheme = static_cast<FilteringScheme>(PtrToUlong(gFilteringSchemeCombo->GetSelectedData()));
    PartitionScheme partitionScheme = static_cast<PartitionScheme>(PtrToUlong(gPartitionSchemeCombo->GetSelectedData()));

    // MSAA only valid with EVSM filtering
    gShadowAACombo->SetEnabled(filteringScheme == EVSM_FILTERING);
    gShadowTransparencyAACheck->SetEnabled(filteringScheme == EVSM_FILTERING);

    // Tight bounds is implicitly always on with SDSM partitions
    gTightPartitionBoundsCheck->SetEnabled(partitionScheme == PSSM_PARTITIONS);

	// Align to frustum cannot be used with quantization
	gAlignLightToFrustumCheck->SetEnabled(!gQuantizePartitionsCheck->GetChecked());
}


void InitApp(ID3D11Device* d3dDevice)
{
    DestroyApp();

    // Grab parameters from UI
    unsigned int partitions = static_cast<unsigned int>(PtrToUlong(gPartitionsCombo->GetSelectedData()));
    unsigned int partitionsPerPass = static_cast<unsigned int>(PtrToUlong(gPartitionsPerPassCombo->GetSelectedData()));
    unsigned int shadowTextureDim = static_cast<unsigned int>(PtrToUlong(gShadowTextureDimCombo->GetSelectedData()));
    unsigned int shadowAASamples = static_cast<unsigned int>(PtrToUlong(gShadowAACombo->GetSelectedData()));
    FilteringScheme filteringScheme = static_cast<FilteringScheme>(PtrToUlong(gFilteringSchemeCombo->GetSelectedData()));

    gApp = new App(d3dDevice, partitions, partitionsPerPass, shadowTextureDim, shadowAASamples,
        gShadowTransparencyAACheck->GetChecked(), filteringScheme);

    // Initialize with the current surface description
    gApp->OnD3D11ResizedSwapChain(d3dDevice, DXUTGetDXGIBackBufferSurfaceDesc());

    // Zero out the elapsed time for the next frame
    gZeroNextFrameTime = true;
}


void DestroyApp()
{
    SAFE_DELETE(gApp);
}

void LoadSkybox(ID3D11Device* d3dDevice, LPCWSTR fileName)
{
    ID3D11Resource* resource = 0;
    HRESULT hr;
    hr = D3DX11CreateTextureFromFile(d3dDevice, fileName, 0, 0, &resource, 0);
    assert(SUCCEEDED(hr));

    d3dDevice->CreateShaderResourceView(resource, 0, &gSkyboxSRV);
    resource->Release();
}

void InitScene(ID3D11Device* d3dDevice)
{
    DestroyScene();

    D3DXVECTOR3 cameraEye(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 cameraAt(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 lightDirection(0.0f, 0.0f, 0.0f);
    float sceneScaling = 1.0f;
    bool zAxisUp = false;

    SCENE_SELECTION scene = static_cast<SCENE_SELECTION>(PtrToUlong(gSceneSelectCombo->GetSelectedData()));
    switch (scene) {
        case POWER_PLANT_SCENE: {
            gMeshOpaque.Create(d3dDevice, L"..\\media\\powerplant\\powerplant.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\Clouds.dds");
            sceneScaling = 1.0f;
            cameraEye = sceneScaling * D3DXVECTOR3(100.0f, 5.0f, 5.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
            lightDirection = D3DXVECTOR3(32.0f, -30.0f, 22.0f);
        } break;
            
        case TOWER_SCENE: {
            gMeshOpaque.Create(d3dDevice, L"..\\media\\Tower\\Tower.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\Clouds.dds");
            sceneScaling = 0.04f;
            cameraEye = sceneScaling * D3DXVECTOR3(3000.0f, 400.0f, -1000.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(1000.0f, 0.0f, -1000.0f);
            lightDirection = D3DXVECTOR3(50.0f, -18.0f, -50.0f);
        } break;
    };
    
    D3DXMatrixScaling(&gWorldMatrix, sceneScaling, sceneScaling, sceneScaling);
    if (zAxisUp) {
        D3DXMATRIXA16 m;
        D3DXMatrixRotationX(&m, -D3DX_PI / 2.0f);
        gWorldMatrix *= m;
    }

    gViewerCamera.SetViewParams(&cameraEye, &cameraAt);
    gViewerCamera.SetScalers(0.01f, 10.0f);
    gViewerCamera.FrameMove(0.0f);

    D3DXVec3Normalize(&gLightDirection, &lightDirection);

    // Zero out the elapsed time for the next frame
    gZeroNextFrameTime = true;
}


void DestroyScene()
{
    gMeshOpaque.Destroy();
    gMeshAlpha.Destroy();
    SAFE_RELEASE(gSkyboxSRV);
}


bool CALLBACK CommandLineArg(const WCHAR* arg, const WCHAR* param, void* userContext)
{
    if (wcscmp(arg, L"filtering") == 0)
	{
        if (wcscmp(param, L"evsm") == 0)
			gDefaultFiltering = EVSM_FILTERING;
		else if (wcscmp(param, L"pcf") == 0)
			gDefaultFiltering = NEIGHBOR_PCF_FILTERING;
	}
	else if (wcscmp(arg, L"partitionscheme") == 0)
	{
		if (wcscmp(param, L"kmeans") == 0)
			gUIConstants.partitionScheme = KMEANS_PARTITIONS;
		else if (wcscmp(param, L"pssm") == 0)
			gUIConstants.partitionScheme = PSSM_PARTITIONS;
		else if (wcscmp(param, L"log") == 0)
			gUIConstants.partitionScheme = LOG_PARTITIONS;
		else if (wcscmp(param, L"logreduce") == 0)
			gUIConstants.partitionScheme = LOG_PARTITIONS_REDUCE;
		else if (wcscmp(param, L"adaptivelog") == 0)
			gUIConstants.partitionScheme = ADAPTIVE_LOG_PARTITIONS;
	}
	else if (wcscmp(arg, L"numpartitions") == 0)
		gDefaultPartitions = _wtoi(param);
	else if (wcscmp(arg, L"shadowdim") == 0)
		gDefaultShadowDim = _wtoi(param);
	else if (wcscmp(arg, L"shadowaasamples") == 0)
		gDefaultShadowAASamples = _wtoi(param);
	else if (wcscmp(arg, L"ui") == 0)
		gDisplayUI = (_wtoi(param) != 0);
    else
        return false;
    return true;
}


bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* deviceSettings, void* userContext)
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        if (deviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE) {
            DXUTDisplaySwitchingToREFWarning(deviceSettings->ver);
        }
    }

    // We don't currently support framebuffer MSAA
    // Requires multi-frequency shading wrt. the GBuffer that is not yet implemented
    deviceSettings->d3d11.sd.SampleDesc.Count = 1;
    deviceSettings->d3d11.sd.SampleDesc.Quality = 0;

    // Also don't need a depth/stencil buffer... we'll manage that ourselves
    deviceSettings->d3d11.AutoCreateDepthStencil = false;

    return true;
}


void CALLBACK OnFrameMove(double time, float elapsedTime, void* userContext)
{
    if (gZeroNextFrameTime) {
        elapsedTime = 0.0f;
    }
    // Flag will be reset in render function below

    // Update the camera's position based on user input
    gViewerCamera.FrameMove(elapsedTime);

    // If requested, orbit the light
    if (gAnimateLightCheck->GetChecked()) {
        D3DXMATRIXA16 rotation;
        D3DXMatrixRotationY(&rotation, kLightRotationSpeed * elapsedTime);
        D3DXVec3TransformNormal(&gLightDirection, &gLightDirection, &rotation);
    }
}


LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* noFurtherProcessing,
                          void* userContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *noFurtherProcessing = gDialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam );
    if (*noFurtherProcessing) {
        return 0;
    }

    // Pass messages to settings dialog if its active
    if (gD3DSettingsDlg.IsActive()) {
        gD3DSettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    for (int i = 0; i < HUD_NUM; ++i) {
        *noFurtherProcessing = gHUD[i].MsgProc(hWnd, uMsg, wParam, lParam);
        if(*noFurtherProcessing) {
            return 0;
        }
    }

    // Pass all remaining windows messages to camera so it can respond to user input
    gViewerCamera.HandleMessages(hWnd, uMsg, wParam, lParam);

    return 0;
}


void CALLBACK OnKeyboard(UINT character, bool keyDown, bool altDown, void* userContext)
{
    if(keyDown) {
        switch (character) {
        case VK_F8:
            // Toggle visibility of expert HUD
            gHUD[HUD_EXPERT].SetVisible(!gHUD[HUD_EXPERT].GetVisible());
            break;
        case VK_F9:
            // Toggle display of UI on/off
            gDisplayUI = !gDisplayUI;
            break;
        }
    }
}


void CALLBACK OnGUIEvent(UINT eventID, INT controlID, CDXUTControl* control, void* userContext)
{
    switch (controlID) {
        case UI_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case UI_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case UI_CHANGEDEVICE:
            gD3DSettingsDlg.SetActive(!gD3DSettingsDlg.IsActive()); break;
        case UI_LIGHTINGONLY:
            gUIConstants.lightingOnly = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_FACENORMALS:
            gUIConstants.faceNormals = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
		case UI_QUANTIZEPARTITIONS:
            gUIConstants.quantizePartitions = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_VISUALIZEPARTITIONS:
            gUIConstants.visualizePartitions = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_VISUALIZEHISTOGRAM:
            gUIConstants.visualizeHistogram = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_VISUALIZEERROR:
            gUIConstants.visualizeError = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_EDGESOFTENING:
            gUIConstants.edgeSoftening = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_EDGESOFTENINGAMOUNT:
            gUIConstants.edgeSofteningAmount =
                static_cast<float>(dynamic_cast<CDXUTSlider*>(control)->GetValue()) /
                kSliderFactorResolution;
            break;
        case UI_SELECTEDSCENE:
            DestroyScene(); break;
        case UI_PARTITIONSCHEME:
            gUIConstants.partitionScheme = static_cast<unsigned int>(PtrToUlong(gPartitionSchemeCombo->GetSelectedData())); break;

        case UI_PSSMFACTOR:
            gUIConstants.pssmFactor =
                static_cast<float>(dynamic_cast<CDXUTSlider*>(control)->GetValue()) /
                kSliderFactorResolution;
            break;
        case UI_TIGHTPARTITIONBOUNDS:
            gUIConstants.tightPartitionBounds = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_ALIGNLIGHTTOFRUSTUM:
            gUIConstants.alignLightToFrustum = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_CPUPARTITIONFRUSTUMCULLING:
            gUIConstants.cpuPartitionFrustumCulling = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_HISTOGRAMRESOLUTIONPOWER:
            gUIConstants.histogramResolutionPower =
                static_cast<float>(dynamic_cast<CDXUTSlider*>(control)->GetValue()) /
                kSliderFactorResolution;
            break;
        case UI_LLOYDITERATIONS:
            gUIConstants.lloydIterations = dynamic_cast<CDXUTSlider*>(control)->GetValue(); break;
        case UI_USEPOSITIVEEXPONENT:
            gUIConstants.usePositiveExponent = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_POSITIVEEXPONENT:
            gUIConstants.positiveExponent =
                static_cast<float>(dynamic_cast<CDXUTSlider*>(control)->GetValue()) /
                kSliderFactorResolution;
            break;
        case UI_USENEGATIVEEXPONENT:
            gUIConstants.useNegativeExponent = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_NEGATIVEEXPONENT:
            gUIConstants.negativeExponent =
                static_cast<float>(dynamic_cast<CDXUTSlider*>(control)->GetValue()) /
                kSliderFactorResolution;
            break;
        case UI_SAVENORMALIZEDDEPTH:
            gApp->SaveNextNormalizedDepth();
            break;
        case UI_VISUALIZELIGHTSPACE:
            gUIConstants.visualizeLightSpace = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;

        // These controls all imply changing parameters to the App constructor
        // (i.e. recreating resources and such), so we'll just clean up the app here and let it be
        // lazily recreated next render.
        case UI_PARTITIONS:
        case UI_PARTITIONSPERPASS:
        case UI_SHADOWTEXTUREDIM:
        case UI_FILTERINGSCHEME:
        case UI_SHADOWAA:
        case UI_SHADOWTRANSPARENCYAA:
            DestroyApp(); break;

        default:
            break;
    }

    UpdateUIState();
}


void CALLBACK OnD3D11DestroyDevice(void* userContext)
{
    DestroyApp();
    DestroyScene();
    
    gDialogResourceManager.OnD3D11DestroyDevice();
    gD3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(gTextHelper);
}


HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferSurfaceDesc,
                                     void* userContext)
{    
    ID3D11DeviceContext* d3dDeviceContext = DXUTGetD3D11DeviceContext();
    gDialogResourceManager.OnD3D11CreateDevice(d3dDevice, d3dDeviceContext);
    gD3DSettingsDlg.OnD3D11CreateDevice(d3dDevice);
    gTextHelper = new CDXUTTextHelper(d3dDevice, d3dDeviceContext, &gDialogResourceManager, 15);
    
    gViewerCamera.SetRotateButtons(true, false, false);
    gViewerCamera.SetDrag(true);
    gViewerCamera.SetEnableYAxisMovement(true);

    return S_OK;
}


HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, IDXGISwapChain* swapChain,
                                          const DXGI_SURFACE_DESC* backBufferSurfaceDesc, void* userContext)
{
    HRESULT hr;

    V_RETURN(gDialogResourceManager.OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc));
    V_RETURN(gD3DSettingsDlg.OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc));

    float gAspectRatio = backBufferSurfaceDesc->Width / (float)backBufferSurfaceDesc->Height;
    // NOTE: Complementary Z (1-z) buffer used here, so swap near/far
    gViewerCamera.SetProjParams(D3DX_PI / 4.0f, gAspectRatio, 300.0f, 0.05f);

    // Standard HUDs
    const int border = 20;
    int y = border;
    for (int i = 0; i < HUD_EXPERT; ++i) {
        gHUD[i].SetLocation(backBufferSurfaceDesc->Width - gHUD[i].GetWidth() - border, y);
        y += gHUD[i].GetHeight() + border;
    }

    // Expert HUD
    gHUD[HUD_EXPERT].SetLocation(border, 80);

    // If there's no app, it'll pick this up when it gets lazily created so just ignore it
    if (gApp) {
        gApp->OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc);
    }

    return S_OK;
}


void CALLBACK OnD3D11ReleasingSwapChain(void* userContext)
{
    gDialogResourceManager.OnD3D11ReleasingSwapChain();
}


void CALLBACK OnD3D11FrameRender(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, double time,
                                 float elapsedTime, void* userContext)
{
    if (gZeroNextFrameTime) {
        elapsedTime = 0.0f;
    }
    gZeroNextFrameTime = false;

    if (gD3DSettingsDlg.IsActive()) {
        gD3DSettingsDlg.OnRender(elapsedTime);
        return;
    }

    // Lazily create the application if need be
    if (!gApp) {
        InitApp(d3dDevice);
    }

    // Lazily load scene
    if (!gMeshOpaque.IsLoaded() && !gMeshAlpha.IsLoaded()) {
        InitScene(d3dDevice);
    }

    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    
    D3D11_VIEWPORT viewport;
    viewport.Width    = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Width);
    viewport.Height   = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

    gApp->Render(d3dDeviceContext, pRTV, gMeshOpaque, gMeshAlpha, gSkyboxSRV,
        gWorldMatrix, &gViewerCamera, gLightDirection, &viewport, &gUIConstants);

    if (gDisplayUI) {
        d3dDeviceContext->RSSetViewports(1, &viewport);

        // Render HUDs in reverse order
        d3dDeviceContext->OMSetRenderTargets(1, &pRTV, 0);
        for (int i = HUD_NUM - 1; i >= 0; --i) {
            gHUD[i].OnRender(elapsedTime);
        }

        // Render text
        gTextHelper->Begin();

        gTextHelper->SetInsertionPos(2, 0);
        gTextHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
        gTextHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
        //gTextHelper->DrawTextLine(DXUTGetDeviceStats());

        // Output frame time
        std::wostringstream oss;
        oss << 1000.0f / DXUTGetFPS() << " ms / frame";
        gTextHelper->DrawTextLine(oss.str().c_str());

        // Output error result if applicable
        if (gUIConstants.visualizeError > 0) {
            std::wostringstream oss;
            oss << L"Average error: " << gApp->GetAverageError();
            gTextHelper->DrawTextLine(oss.str().c_str());
        }

        gTextHelper->End();
    }
}
