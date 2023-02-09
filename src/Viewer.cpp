//
// Game.cpp
//

#include "pch.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

#include "Util.h"
#include "Viewer.h"

extern void ExitViewer() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const XMVECTORF32 ROOM_BOUNDS = { 8.f, 6.f, 12.f, 0.f };
    constexpr float ROTATION_GAIN = 0.02f;
    constexpr float MOVEMENT_GAIN = 0.07f;
    
    constexpr float c_defaultTheta = 0;
    constexpr float c_defaultPhi = XM_2PI / 5.0f;
    constexpr float c_defaultRadius = 3.3f;
    constexpr float c_minRadius = 0.1f;
    constexpr float c_maxRadius = 5.f;
}

// Pix debugging
static std::wstring GetLatestWinPixGpuCapturerPath()
{
    LPWSTR programFilesPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

    std::wstring pixSearchPath = programFilesPath + std::wstring(L"\\Microsoft PIX\\*");

    WIN32_FIND_DATA findData;
    bool foundPixInstallation = false;
    wchar_t newestVersionFound[MAX_PATH];

    HANDLE hFind = FindFirstFile(pixSearchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) &&
                (findData.cFileName[0] != '.'))
            {
                if (!foundPixInstallation || wcscmp(newestVersionFound, findData.cFileName) <= 0)
                {
                    foundPixInstallation = true;
                    StringCchCopy(newestVersionFound, _countof(newestVersionFound), findData.cFileName);
                }
            }
        } while (FindNextFile(hFind, &findData) != 0);
    }

    FindClose(hFind);

    if (!foundPixInstallation)
    {
        // TODO: Error, no PIX installation found
    }

    wchar_t output[MAX_PATH];
    StringCchCopy(output, pixSearchPath.length(), pixSearchPath.data());
    StringCchCat(output, MAX_PATH, &newestVersionFound[0]);
    StringCchCat(output, MAX_PATH, L"\\WinPixGpuCapturer.dll");

    return &output[0];
}

Viewer::Viewer() noexcept(false)
{
    if (GetModuleHandle(L"WinPixGpuCapturer.dll") == 0)
    {
        LoadLibrary(GetLatestWinPixGpuCapturerPath().c_str());
    }

    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);

    m_theta = c_defaultTheta;
    m_phi = c_defaultPhi;
    m_radius = c_defaultRadius;
}

Viewer::~Viewer()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
std::unique_ptr<DX::DeviceResources>& Viewer::Initialize(HWND window, int width, int height, const wchar_t* modelPath)
{
    viewerModel = ViewerModel(modelPath);
    
    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_keyboard = std::make_unique<Keyboard>();
    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(window);
    
    return m_deviceResources;
}

#pragma region Frame Update
// Executes the basic game loop.
void Viewer::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Viewer::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    viewerModel.Update(timer);

    auto mouse = m_mouse->GetState();
    m_mouseButtons.Update(mouse);

    m_radius -= float(mouse.scrollWheelValue) * ROTATION_GAIN;
    m_mouse->ResetScrollWheelValue();
    m_radius = std::max(c_minRadius, std::min(c_maxRadius, m_radius));

    if (mouse.positionMode == Mouse::MODE_RELATIVE)
    {
        Vector3 delta = Vector3(float(mouse.x), float(mouse.y), 0.f) * ROTATION_GAIN;

        m_phi -= delta.y;
        m_theta += delta.x;
    }

    m_mouse->SetMode(mouse.leftButton ?
        Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);

    auto kb = m_keyboard->GetState();
    m_keys.Update(kb);

    if (kb.Escape)
    {
        ExitViewer();
    }

    constexpr float limit = XM_PIDIV2 - 0.01f;
    m_phi = std::max(1e-2f, std::min(limit, m_phi));

    if (m_theta > XM_PI)
    {
        m_theta -= XM_2PI;
    }
    else if (m_theta < -XM_PI)
    {
        m_theta += XM_2PI;
    }

    XMVECTOR lookFrom = XMVectorSet(
        m_radius * sinf(m_phi) * cosf(m_theta),
        m_radius * cosf(m_phi),
        m_radius * sinf(m_phi) * sinf(m_theta),
        0);

    m_view = XMMatrixLookAtRH(lookFrom, g_XMZero, Vector3::Up);

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Viewer::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Render ImGui
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin(Util::WStringToString(viewerModel.GetModelName()).c_str());
	ImGui::Text("Orbit camera with mouse");
    std::vector<std::wstring> animations = viewerModel.GetAnimationNames();
    if (animations.size() > 0)
    {
        ImGui::Text("Choose animation");
        int count = 0;
        for (auto it = begin(animations); it != end(animations); ++it) {
            if (ImGui::Button(Util::WStringToString(*it).c_str())) {
                viewerModel.SetAnimation(count);
            }
            count++;
        }
    }
    else
    {
        ImGui::Text("No animations");
    }
    ImGui::End();
    ImGui::Render();
    
    // Render model
    viewerModel.Render(commandList, m_world, m_view, m_proj);

    auto srvDh = m_deviceResources->GetD3DDescriptorHeap();
	auto targetView = m_deviceResources->GetRenderTargetView();
	auto stencilView = m_deviceResources->GetDepthStencilView();
	commandList->OMSetRenderTargets(1, &targetView, FALSE, &stencilView);
    commandList->SetDescriptorHeaps(1, &srvDh);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
    
    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    PIXEndEvent();

}

// Helper method to clear the back buffers.
void Viewer::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto const rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto const dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto const viewport = m_deviceResources->GetScreenViewport();
    auto const scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Viewer::OnActivated()
{
    m_keys.Reset();
    m_mouseButtons.Reset();
}

void Viewer::OnDeactivated()
{
    // TODO: Viewer is becoming background window.
}

void Viewer::OnSuspending()
{
    // TODO: Viewer is being power-suspended (or minimized).
}

void Viewer::OnResuming()
{
    m_timer.ResetElapsedTime();

    m_keys.Reset();
    m_mouseButtons.Reset();
}

void Viewer::OnWindowMoved()
{
    auto const r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Viewer::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Viewer::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

// Properties
void Viewer::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Viewer::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    DXGI_FORMAT backBufferFormat = m_deviceResources->GetBackBufferFormat();
    DXGI_FORMAT depthBufferFormat = m_deviceResources->GetDepthBufferFormat();
    ID3D12CommandQueue* commandQueue = m_deviceResources->GetCommandQueue();

    // Check Shader Model 6 support
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_0 };
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))) || (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0))
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: Shader Model 6.0 is not supported!\n");
#endif
        throw std::runtime_error("Shader Model 6.0 is not supported!");
    }

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    viewerModel.CreateDeviceDependentResources(device, backBufferFormat, depthBufferFormat, commandQueue);

    m_world = Matrix::Identity;
}

// Allocate all memory resources that change on a window SizeChanged event.
void Viewer::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    m_proj = Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.f, float(size.right) / float(size.bottom), 0.1f, 1000.f);
}

void Viewer::OnDeviceLost()
{
    m_graphicsMemory.reset();
    
    viewerModel.OnDeviceLost();

}

void Viewer::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
